/*
htop - MemoryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "MemoryMeter.h"

#include <math.h>
#include <stddef.h>

#include "CRT.h"
#include "Debug.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"


extern const int Platform_memoryMeter_attributes[]; // OS-specific


static void MemoryMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;

   Settings *settings = this->host->settings;

   /* not all memory classes are supported on all platforms */
   for (unsigned int memoryClassIdx = 0; memoryClassIdx < Platform_numberOfMemoryClasses; memoryClassIdx++) {
      this->values[memoryClassIdx] = NAN;
   }

   Platform_setMemoryValues(this);
   this->curItems = (uint8_t) Platform_numberOfMemoryClasses;

   /* compute the used memory */
   double used = 0.0;
   for (unsigned int memoryClassIdx = 0; memoryClassIdx < Platform_numberOfMemoryClasses; memoryClassIdx++) {
      if (Platform_memoryClasses[memoryClassIdx].countsAsUsed) {
         used += this->values[memoryClassIdx];
      }
   }

   /* clear the values we don't want to see */
   if ((this->mode == GRAPH_METERMODE || this->mode == BAR_METERMODE) && !settings->showCachedMemory) {
      for (unsigned int memoryClassIdx = 0; memoryClassIdx < Platform_numberOfMemoryClasses; memoryClassIdx++) {
         if (Platform_memoryClasses[memoryClassIdx].countsAsCache) {
            this->values[memoryClassIdx] = NAN;
         }
      }
   }

   written = Meter_humanUnit(buffer, used, size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void MemoryMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;
   const Settings* settings = this->host->settings;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   /* print the OS-specific memory classes in the order supplied by their implementation */
   for (unsigned int memoryClassIdx = 0; memoryClassIdx < Platform_numberOfMemoryClasses; memoryClassIdx++) {
      if (!settings->showCachedMemory && Platform_memoryClasses[memoryClassIdx].countsAsCache)
         continue; // skip reclaimable cache memory classes if "show cached memory" is not ticked

      Meter_humanUnit(buffer, this->values[memoryClassIdx], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], Platform_memoryClasses[memoryClassIdx].label);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], ":");
      RichString_appendAscii(out, CRT_colors[Platform_memoryClasses[memoryClassIdx].color], buffer);
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
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = NUMBER_OF_DYNAMIC_COLORS, // all the range of DYNAMIC_xxxx colors are allowed
   .isPercentChart = true,
   .total = 100.0,
   .attributes = Platform_memoryMeter_attributes, // OS-specific
   .name = "Memory",
   .uiName = "Memory",
   .caption = "Mem"
};
