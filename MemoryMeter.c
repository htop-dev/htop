/*
htop - MemoryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "MemoryMeter.h"

#include <math.h>
#include <stddef.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"


static const int MemoryMeter_attributes[] = {
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_SHARED,
   MEMORY_CACHE
};

static void MemoryMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;

   /* shared and available memory are not supported on all platforms */
   this->values[MEMORY_METER_SHARED] = NAN;
   this->values[MEMORY_METER_AVAILABLE] = NAN;
   Platform_setMemoryValues(this);

   /* Do not print available memory in bar mode */
   this->curItems = 4;

   written = Meter_humanUnit(buffer, this->values[MEMORY_METER_USED], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void MemoryMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   Meter_humanUnit(buffer, this->values[MEMORY_METER_USED], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   RichString_appendAscii(out, CRT_colors[MEMORY_USED], buffer);

   Meter_humanUnit(buffer, this->values[MEMORY_METER_BUFFERS], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " buffers:");
   RichString_appendAscii(out, CRT_colors[MEMORY_BUFFERS_TEXT], buffer);

   /* shared memory is not supported on all platforms */
   if (!isnan(this->values[MEMORY_METER_SHARED])) {
      Meter_humanUnit(buffer, this->values[MEMORY_METER_SHARED], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " shared:");
      RichString_appendAscii(out, CRT_colors[MEMORY_SHARED], buffer);
   }

   Meter_humanUnit(buffer, this->values[MEMORY_METER_CACHE], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " cache:");
   RichString_appendAscii(out, CRT_colors[MEMORY_CACHE], buffer);

   /* available memory is not supported on all platforms */
   if (!isnan(this->values[MEMORY_METER_AVAILABLE])) {
      Meter_humanUnit(buffer, this->values[MEMORY_METER_AVAILABLE], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " available:");
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
   }
}

const MeterClass MemoryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = MemoryMeter_display,
   },
   .updateValues = MemoryMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 5,
   .total = 100.0,
   .attributes = MemoryMeter_attributes,
   .name = "Memory",
   .uiName = "Memory",
   .caption = "Mem"
};
