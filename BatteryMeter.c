/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "config.h" // IWYU pragma: keep

#include "BatteryMeter.h"

#include <math.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "XUtils.h"


static const int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_updateValues(Meter* this) {
   BatteryInfo info = {
      .ac = AC_ERROR,
      .percent = NAN,
      .powerCurr = NAN,
      .energyCurr = NAN,
      .energyFull = NAN,
   };

   Platform_getBattery(&info);

   if (!isNonnegative(info.percent)) {
      this->values[0] = NAN;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      return;
   }

   info.percent = CLAMP(info.percent, 0.0, 100.0);
   this->values[0] = info.percent;

   bool havePower = isfinite(info.powerCurr);
   bool haveEnergy = isNonnegative(info.energyCurr) && isNonnegative(info.energyFull);

   /* Without energy data there is nothing useful to show beyond the percent. */
   if (!haveEnergy) {
      if (havePower) {
         if (info.ac == AC_PRESENT) {
            xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "AC %+.1fW, %.1f%%", info.powerCurr, info.percent);
         } else if (info.ac == AC_ABSENT) {
            xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "bat %+.1fW, %.1f%%", info.powerCurr, info.percent);
         } else {
            xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%+.1fW, %.1f%%", info.powerCurr, info.percent);
         }
      } else if (info.ac == AC_PRESENT) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "AC %.1f%%", info.percent);
      } else if (info.ac == AC_ABSENT) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "bat %.1f%%", info.percent);
      } else {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", info.percent);
      }
      return;
   }

   /* stable: power unknown or |power| < 5 W – time estimate would be unreliable */
   bool isDischarging = havePower && info.powerCurr <= -5.0;
   bool isCharging = havePower && info.powerCurr >= 5.0;

   /* time estimate in whole minutes; -1 means not available */
   int timeMinutes = -1;
   if (isDischarging && isPositive(info.energyCurr)) {
      /* floor for discharge; powerCurr is negative, use negative scaling when dividing */
      timeMinutes = (int)floor(info.energyCurr / info.powerCurr * -60.0);
   } else if (isCharging && 0.95 * info.energyFull > info.energyCurr) {
      /* ceil for charge; uses 95% to avoid slow charging tail near full */
      timeMinutes = (int)ceil((0.95 * info.energyFull - info.energyCurr) / info.powerCurr * 60.0);
   }

   char* buf = this->txtBuffer;
   size_t len = sizeof(this->txtBuffer);
   int ret = 0;

   if (this->mode == TEXT_METERMODE) {
      if (info.ac == AC_PRESENT) {
         ret = xSnprintf(buf, len, "Using %s", isDischarging ? "AC+bat" : "AC");
         buf += ret; len -= ret;
      } else if (info.ac == AC_ABSENT) {
         ret = xSnprintf(buf, len, "Using bat");
         buf += ret; len -= ret;
      }

      if (ret && len > 2) {
         *buf++ = ',';
         *buf++ = ' ';
         *buf = 0;
         len -= 2;
      }

      if (isDischarging) {
         ret = xSnprintf(
            buf, len, "discharging at %.1fW, %.1f/%.1fWh (%.1f%%)",
            -info.powerCurr, info.energyCurr, info.energyFull, info.percent
         );
         buf += ret; len -= ret;
         if (timeMinutes >= 0) {
            ret = xSnprintf(buf, len, ", time remaining: %dh%02dm", timeMinutes / 60, timeMinutes % 60);
            buf += ret; len -= ret;
         }
      } else if (isCharging) {
         ret = xSnprintf(
            buf, len, "charging at %.1fW, %.1f/%.1fWh (%.1f%%)",
            info.powerCurr, info.energyCurr, info.energyFull, info.percent
         );
         buf += ret; len -= ret;

         if (timeMinutes >= 0) {
            ret = xSnprintf(
               buf, len, ", time to full: %dh%02dm",
               timeMinutes / 60, timeMinutes % 60
            );
            buf += ret; len -= ret;
         }
      } else {
         ret = xSnprintf(
            buf, len, "stable at %.1f/%.1fWh (%.1f%%)",
            info.energyCurr, info.energyFull, info.percent
         );
         buf += ret; len -= ret;
      }
   } else {
      /* compact label for bar / graph modes */
      if (info.ac == AC_PRESENT) {
         ret = xSnprintf(buf, len, "%s", isDischarging ? "AC+bat" : "AC");
         buf += ret; len -= ret;
      } else if (info.ac == AC_ABSENT) {
         ret = xSnprintf(buf, len, "bat");
         buf += ret; len -= ret;
      }

      if (ret && len > 1) {
         *buf++ = ' ';
         *buf = 0;
         len--;
      }

      if (isCharging || isDischarging) {
         ret = xSnprintf(
            buf, len, "%+.1fW @ %.1f/%.1fWh",
            info.powerCurr, info.energyCurr, info.energyFull
         );
         buf += ret; len -= ret;

         if (timeMinutes >= 0) {
            ret = xSnprintf(
               buf, len, ", %dh%02dm",
               timeMinutes / 60, timeMinutes % 60
            );
            buf += ret; len -= ret;
         }
      } else {
         ret = xSnprintf(
            buf, len, "stable @ %.1f/%.1fWh",
            info.energyCurr, info.energyFull
         );
         buf += ret; len -= ret;
      }
   }

   // Avoid compiler warning about unused variables
   (void)ret;
   (void)buf;
   (void)len;
}

const MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = BatteryMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .isPercentChart = true,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
