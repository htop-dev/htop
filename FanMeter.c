/*
htop - FanMeter.c
(C) 2026 Murad Karammaev
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FanMeter.h"

#include "CRT.h"
#include "Platform.h"
#include "XUtils.h"


static const int FanMeter_attributes[] = {
   METER_VALUE
};

static void FanMeter_updateValues(Meter* this) {
   int fanspeed = Platform_getFanSpeed();
   if (fanspeed < 0) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%d rpm", fanspeed);
   }
}

const MeterClass FanMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = FanMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = FanMeter_attributes,
   .name = "Fan",
   .uiName = "Fan speed",
   .caption = "Fan speed: ",
};
