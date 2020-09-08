/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include "Battery.h"
#include "ProcessList.h"
#include "CRT.h"
#include "StringUtils.h"
#include "Platform.h"

#include <string.h>
#include <stdlib.h>


int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_updateValues(Meter * this, char *buffer, int len) {
   ACPresence isOnAC;
   double percent;

   Battery_getData(&percent, &isOnAC);

   if (percent == -1) {
      this->values[0] = 0;
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

   return;
}

MeterClass BatteryMeter_class = {
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
