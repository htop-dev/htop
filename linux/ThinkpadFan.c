/*
htop - ThinkpadFan.c
(C) 2026 Murad Karammaev
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/ThinkpadFan.h"

#include "CRT.h"
#include "Object.h"
#include "XUtils.h"
#include "linux/Platform.h"


static const int ThinkpadFanMeter_attributes[] = {
   METER_VALUE
};

static void ThinkpadFanMeter_updateValues(Meter* this) {
   int fanspeed = Platform_getThinkpadFan();
   if (fanspeed < 0)
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   else
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%d RPM", fanspeed);
}

const MeterClass ThinkpadFanMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = ThinkpadFanMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = ThinkpadFanMeter_attributes,
   .name = "ThinkpadFan",
   .uiName = "Thinkpad fan speed",
   .caption = "Thinkpad fan: ",
};
