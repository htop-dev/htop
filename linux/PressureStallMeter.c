/*
htop - PressureStallMeter.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2019 Ran Benita
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/PressureStallMeter.h"

#include <stdbool.h>
#include <string.h>

#include "CRT.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int PressureStallMeter_attributes[] = {
   PRESSURE_STALL_TEN,
   PRESSURE_STALL_SIXTY,
   PRESSURE_STALL_THREEHUNDRED
};

static void PressureStallMeter_updateValues(Meter* this) {
   const char* file;
   if (strstr(Meter_name(this), "CPU")) {
      file = "cpu";
   } else if (strstr(Meter_name(this), "IO")) {
      file = "io";
   } else {
      file = "memory";
   }

   bool some;
   if (strstr(Meter_name(this), "Some")) {
      some = true;
   } else {
      some = false;
   }

   Platform_getPressureStall(file, some, &this->values[0], &this->values[1], &this->values[2]);

   /* only print bar for ten (not sixty and threehundred), cause the sum is meaningless */
   this->curItems = 1;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s %s %5.2lf%% %5.2lf%% %5.2lf%%", some ? "some" : "full", file, this->values[0], this->values[1], this->values[2]);
}

static void PressureStallMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%5.2lf%% ", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[PRESSURE_STALL_TEN], buffer, len);
   len = xSnprintf(buffer, sizeof(buffer), "%5.2lf%% ", this->values[1]);
   RichString_appendnAscii(out, CRT_colors[PRESSURE_STALL_SIXTY], buffer, len);
   len = xSnprintf(buffer, sizeof(buffer), "%5.2lf%% ", this->values[2]);
   RichString_appendnAscii(out, CRT_colors[PRESSURE_STALL_THREEHUNDRED], buffer, len);
}

const MeterClass PressureStallCPUSomeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = PressureStallMeter_display,
   },
   .updateValues = PressureStallMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = PressureStallMeter_attributes,
   .name = "PressureStallCPUSome",
   .uiName = "PSI some CPU",
   .caption = "PSI some CPU:    ",
   .description = "Pressure Stall Information, some cpu"
};

const MeterClass PressureStallIOSomeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = PressureStallMeter_display,
   },
   .updateValues = PressureStallMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = PressureStallMeter_attributes,
   .name = "PressureStallIOSome",
   .uiName = "PSI some IO",
   .caption = "PSI some IO:     ",
   .description = "Pressure Stall Information, some io"
};

const MeterClass PressureStallIOFullMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = PressureStallMeter_display,
   },
   .updateValues = PressureStallMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = PressureStallMeter_attributes,
   .name = "PressureStallIOFull",
   .uiName = "PSI full IO",
   .caption = "PSI full IO:     ",
   .description = "Pressure Stall Information, full io"
};

const MeterClass PressureStallMemorySomeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = PressureStallMeter_display,
   },
   .updateValues = PressureStallMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = PressureStallMeter_attributes,
   .name = "PressureStallMemorySome",
   .uiName = "PSI some memory",
   .caption = "PSI some memory: ",
   .description = "Pressure Stall Information, some memory"
};

const MeterClass PressureStallMemoryFullMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = PressureStallMeter_display,
   },
   .updateValues = PressureStallMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = PressureStallMeter_attributes,
   .name = "PressureStallMemoryFull",
   .uiName = "PSI full memory",
   .caption = "PSI full memory: ",
   .description = "Pressure Stall Information, full memory"
};
