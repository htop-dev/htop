/*
htop - Battery.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Battery.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "BatteryMeter.h"
#include "Macros.h"


/* Reference voltage for energy<->charge conversion. Design preferred so
 * that ENERGY_FULL on an aged pack stays comparable to historical readings;
 * VOLTAGE_NOW is the live fallback. */
static double referenceVoltage(const BatteryRaw* bat) {
   if (bat->voltageDesign > 0)
      return bat->voltageDesign;
   if (bat->voltageNow > 0)
      return bat->voltageNow;
   return NAN;
}

/* Voltage to use when computing P = I * V from instantaneous current.
 * Live voltage gives the most accurate instantaneous power; design is the
 * fallback when the kernel exposes only nominal voltage. */
static double instantaneousVoltage(const BatteryRaw* bat) {
   if (bat->voltageNow > 0)
      return bat->voltageNow;
   if (bat->voltageDesign > 0)
      return bat->voltageDesign;
   return NAN;
}

void Battery_aggregate(const BatteryRaw* raws, size_t n, BatteryInfo* out) {
   out->percent = NAN;
   out->powerCurr = NAN;
   out->energyCurr = NAN;
   out->energyFull = NAN;

   if (n == 0)
      return;

   double sumEnergyNow = 0, sumEnergyFull = 0;
   double sumChargeNow = 0, sumChargeFull = 0;
   double sumLevel = 0;
   double sumPower = 0;

   size_t energyContrib = 0;
   size_t chargeContrib = 0;
   size_t levelContrib = 0;
   size_t powerContrib = 0;

   for (size_t i = 0; i < n; i++) {
      BatteryRaw bat = raws[i];

      /* Derive missing now-counter from FULL * level / 100 when the kernel
       * exposes only a percent reading. Firmware sometimes overshoots 100%
       * by a fraction during calibration; clamp before using as a fraction
       * so the per-battery MINIMIUM(now, full) below still produces a
       * sensible reading rather than dropping the contribution. */
      if (bat.level >= 0) {
         double levelClamped = CLAMP(bat.level, 0.0, 100.0);
         if (bat.energyFull > 0 && !(bat.energyNow >= 0))
            bat.energyNow = bat.energyFull * (levelClamped / 100.0);
         if (bat.chargeFull > 0 && !(bat.chargeNow >= 0))
            bat.chargeNow = bat.chargeFull * (levelClamped / 100.0);
      }

      /* Derive energy from charge * voltage when energy is missing on this
       * unit but charge + a usable voltage are available. Done per-unit so
       * mixed packs (one energy-reporting, one charge-reporting) still
       * aggregate on a single dimension. */
      double refV = referenceVoltage(&bat);
      if (refV > 0
            && !(bat.energyFull > 0 && bat.energyNow >= 0)
            && bat.chargeFull > 0 && bat.chargeNow >= 0) {
         bat.energyFull = bat.chargeFull * refV;
         bat.energyNow  = bat.chargeNow  * refV;
      }

      if (bat.energyFull > 0 && bat.energyNow >= 0) {
         sumEnergyFull += bat.energyFull;
         sumEnergyNow  += MINIMUM(bat.energyNow, bat.energyFull);
         energyContrib++;
      }

      if (bat.chargeFull > 0 && bat.chargeNow >= 0) {
         sumChargeFull += bat.chargeFull;
         sumChargeNow  += MINIMUM(bat.chargeNow, bat.chargeFull);
         chargeContrib++;
      }

      if (bat.level >= 0) {
         sumLevel += CLAMP(bat.level, 0.0, 100.0);
         levelContrib++;
      }

      /* Derive power from current * voltage when power is missing.
       * Zero current implies zero power even when voltage is unknown. */
      if (!isfinite(bat.power) && isfinite(bat.current)) {
         double pV = instantaneousVoltage(&bat);
         if (pV > 0)
            bat.power = bat.current * pV;
         else if (fpclassify(bat.current) == FP_ZERO)
            bat.power = 0;
      }

      if (isfinite(bat.power)) {
         sumPower += bat.power;
         powerContrib++;
      }
   }

   /* All-or-nothing completeness: every present battery must contribute
    * on the same dimension or the aggregate stays NaN. Mixing partial
    * energy+charge sums is dimensionally invalid. */
   bool energyComplete = (energyContrib == n);
   bool chargeComplete = (chargeContrib == n);
   bool levelComplete  = (levelContrib  == n);
   bool powerComplete  = (powerContrib  == n);

   if (energyComplete && sumEnergyFull > 0) {
      out->energyCurr = sumEnergyNow;
      out->energyFull = sumEnergyFull;
      out->percent    = CLAMP((sumEnergyNow / sumEnergyFull) * 100.0, 0.0, 100.0);
   } else if (chargeComplete && sumChargeFull > 0) {
      out->percent    = CLAMP((sumChargeNow / sumChargeFull) * 100.0, 0.0, 100.0);
   } else if (levelComplete) {
      out->percent    = CLAMP(sumLevel / (double) n, 0.0, 100.0);
   }

   if (powerComplete) {
      out->powerCurr = sumPower;
   }
}
