/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include <math.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "XUtils.h"


static const int BatteryMeter_attributes[] = {
   BATTERY
};

static void BatteryMeter_updateValues(Meter* this) {
   ACPresence isOnAC;
   double percent;

   Platform_getBattery(&percent, &isOnAC);

   if (isnan(percent)) {
      this->values[0] = NAN;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      return;
   }

   this->values[0] = percent;

   const char* text;
   switch (isOnAC) {
   case AC_PRESENT:
      text = this->mode == TEXT_METERMODE ? " (Running on A/C)" : "(A/C)";
      break;
   case AC_ABSENT:
      text = this->mode == TEXT_METERMODE ? " (Running on battery)" : "(bat)";
      break;
   case AC_ERROR:
   default:
      text = "";
      break;
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%%s", percent, text);
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
