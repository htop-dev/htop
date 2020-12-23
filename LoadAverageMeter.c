/*
htop - LoadAverageMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadAverageMeter.h"

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int LoadAverageMeter_attributes[] = {
   LOAD_AVERAGE_ONE,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_FIFTEEN
};

static const int LoadMeter_attributes[] = {
   LOAD
};

static const int OK_attributes[] = {
   METER_VALUE_OK
};

static const int Medium_attributes[] = {
   METER_VALUE_WARN
};

static const int High_attributes[] = {
   METER_VALUE_ERROR
};

static void LoadAverageMeter_updateValues(Meter* this, char* buffer, size_t size) {
   Platform_getLoadAverage(&this->values[0], &this->values[1], &this->values[2]);

   // only show bar for 1min value
   this->curItems = 1;

   // change bar color and total based on value
   if (this->values[0] < 1.0) {
      this->curAttributes = OK_attributes;
      this->total = 1.0;
   } else if (this->values[0] < this->pl->cpuCount) {
      this->curAttributes = Medium_attributes;
      this->total = this->pl->cpuCount;
   } else {
      this->curAttributes = High_attributes;
      this->total = 2 * this->pl->cpuCount;
   }

   xSnprintf(buffer, size, "%.2f/%.2f/%.2f", this->values[0], this->values[1], this->values[2]);
}

static void LoadAverageMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[0]);
   RichString_writeAscii(out, CRT_colors[LOAD_AVERAGE_ONE], buffer);
   xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[1]);
   RichString_appendAscii(out, CRT_colors[LOAD_AVERAGE_FIVE], buffer);
   xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[2]);
   RichString_appendAscii(out, CRT_colors[LOAD_AVERAGE_FIFTEEN], buffer);
}

static void LoadMeter_updateValues(Meter* this, char* buffer, size_t size) {
   double five, fifteen;
   Platform_getLoadAverage(&this->values[0], &five, &fifteen);

   // change bar color and total based on value
   if (this->values[0] < 1.0) {
      this->curAttributes = OK_attributes;
      this->total = 1.0;
   } else if (this->values[0] < this->pl->cpuCount) {
      this->curAttributes = Medium_attributes;
      this->total = this->pl->cpuCount;
   } else {
      this->curAttributes = High_attributes;
      this->total = 2 * this->pl->cpuCount;
   }

   xSnprintf(buffer, size, "%.2f", this->values[0]);
}

static void LoadMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[0]);
   RichString_writeAscii(out, CRT_colors[LOAD], buffer);
}

const MeterClass LoadAverageMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadAverageMeter_display,
   },
   .updateValues = LoadAverageMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage",
   .uiName = "Load average",
   .description = "Load averages: 1 minute, 5 minutes, 15 minutes",
   .caption = "Load average: "
};

const MeterClass LoadMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LoadMeter_display,
   },
   .updateValues = LoadMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = LoadMeter_attributes,
   .name = "Load",
   .uiName = "Load",
   .description = "Load: average of ready processes in the last minute",
   .caption = "Load: "
};
