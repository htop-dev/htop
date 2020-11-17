/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include <math.h>

#include "Platform.h"
#include "CRT.h"
#include "Object.h"
#include "XUtils.h"


static const int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_updateValues(Meter* this, char* buffer, int len) {
   ACPresence isOnAC;
   double percent;

   Platform_getBattery(&percent, &isOnAC);

   if (isnan(percent)) {
      this->values[0] = NAN;
      xSnprintf(buffer, len, "n/a");
      return;
   }

   this->values[0] = percent;

   const char *onAcText, *onBatteryText, *unknownText;

   unknownText = "%.1f%%";
   if (this->mode == TEXT_METERMODE) {
      onAcText = "%.1f%% (Running on A/C)";
      onBatteryText = "%.1f%% (Running on battery)";
   } else {
      onAcText = "%.1f%%(A/C)";
      onBatteryText = "%.1f%%(bat)";
   }

   if (isOnAC == AC_PRESENT) {
      xSnprintf(buffer, len, onAcText, percent);
   } else if (isOnAC == AC_ABSENT) {
      xSnprintf(buffer, len, onBatteryText, percent);
   } else {
      xSnprintf(buffer, len, unknownText, percent);
   }
}

const MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = BatteryMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
