/*
htop - PressureStallMeter.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2019 Ran Benita
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "PressureStallMeter.h"
#include "Platform.h"
#include "CRT.h"

#include <string.h>

/*{
#include "Meter.h"
}*/

static int PressureStallMeter_attributes[] = {
   PRESSURE_STALL_TEN, PRESSURE_STALL_SIXTY, PRESSURE_STALL_THREEHUNDRED
};

static void PressureStallMeter_updateValues(Meter* this, char* buffer, int len) {
    const char *file;
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
    xSnprintf(buffer, len, "xxxx %.2lf%% %.2lf%% %.2lf%%", this->values[0], this->values[1], this->values[2]);
}

static void PressureStallMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   xSnprintf(buffer, sizeof(buffer), "%.2lf%% ", this->values[0]);
   RichString_write(out, CRT_colors[PRESSURE_STALL_TEN], buffer);
   xSnprintf(buffer, sizeof(buffer), "%.2lf%% ", this->values[1]);
   RichString_append(out, CRT_colors[PRESSURE_STALL_SIXTY], buffer);
   xSnprintf(buffer, sizeof(buffer), "%.2lf%% ", this->values[2]);
   RichString_append(out, CRT_colors[PRESSURE_STALL_THREEHUNDRED], buffer);
}

MeterClass PressureStallCPUSomeMeter_class = {
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
   .uiName = "Pressure Stall Information, some CPU",
   .caption = "Some CPU pressure: "
};

MeterClass PressureStallIOSomeMeter_class = {
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
   .uiName = "Pressure Stall Information, some IO",
   .caption = "Some IO  pressure: "
};

MeterClass PressureStallIOFullMeter_class = {
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
   .uiName = "Pressure Stall Information, full IO",
   .caption = "Full IO  pressure: "
};

MeterClass PressureStallMemorySomeMeter_class = {
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
   .uiName = "Pressure Stall Information, some memory",
   .caption = "Some Mem pressure: "
};

MeterClass PressureStallMemoryFullMeter_class = {
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
   .uiName = "Pressure Stall Information, full memory",
   .caption = "Full Mem pressure: "
};
