/*
htop - HugePageMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "HugePageMeter.h"

#include "LinuxProcessList.h"

#include "CRT.h"
#include "Object.h"
#include "RichString.h"


static const int HugePageMeter_attributes[] = {
   MEMORY_USED,
};

static void HugePageMeter_updateValues(Meter* this, char* buffer, size_t size) {
   int written;

   const LinuxProcessList* lpl = (const LinuxProcessList*) this->pl;
   this->total = lpl->totalHugePageMem;
   this->values[0] = lpl->totalHugePageMem - lpl->freeHugePageMem;

   written = Meter_humanUnit(buffer, this->values[0], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void HugePageMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   Meter_humanUnit(buffer, this->values[0], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[MEMORY_USED], buffer);
}

const MeterClass HugePageMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = HugePageMeter_display,
   },
   .updateValues = HugePageMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = HugePageMeter_attributes,
   .name = "HugePages",
   .uiName = "HugePages",
   .caption = "HP"
};
