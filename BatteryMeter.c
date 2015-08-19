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

/*{
#include "Meter.h"

typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;
}*/

int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_setValues(Meter * this, char *buffer, int len) {
   ACPresence isOnAC;
   double percent;
   
   Battery_getData(&percent, &isOnAC);

   if (percent == -1) {
      this->values[0] = 0;
      snprintf(buffer, len, "n/a");
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
      snprintf(buffer, len, onAcText, percent);
   } else if (isOnAC == AC_ABSENT) {
      snprintf(buffer, len, onBatteryText, percent);
   } else {
      snprintf(buffer, len, unknownText, percent);
   }

   return;
}

MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .setValues = BatteryMeter_setValues,
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
