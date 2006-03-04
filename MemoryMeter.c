/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MemoryMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>

#include "debug.h"
#include <assert.h>

/*{

typedef struct MemoryMeter_ MemoryMeter;

struct MemoryMeter_ {
   Meter super;
   ProcessList* pl;
   char* wideFormat;
   int wideLimit;
};

}*/

MemoryMeter* MemoryMeter_new(ProcessList* pl) {
   MemoryMeter* this = malloc(sizeof(MemoryMeter));
   Meter_init((Meter*)this, String_copy("Memory"), String_copy("Mem"), 3);
   ((Meter*)this)->attributes[0] = MEMORY_USED;
   ((Meter*)this)->attributes[1] = MEMORY_BUFFERS;
   ((Meter*)this)->attributes[2] = MEMORY_CACHE;
   ((Meter*)this)->setValues = MemoryMeter_setValues;
   ((Object*)this)->display = MemoryMeter_display;
   this->pl = pl;
   Meter_setMode((Meter*)this, BAR);
   this->wideFormat = "%6ldk ";
   this->wideLimit = 22 + 8 * 4;
   return this;
}

void MemoryMeter_setValues(Meter* cast) {
   MemoryMeter* this = (MemoryMeter*)cast;

   double totalMem = (double)this->pl->totalMem;
   long int usedMem = this->pl->usedMem;
   long int buffersMem = this->pl->buffersMem;
   long int cachedMem = this->pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   cast->total = totalMem;
   cast->values[0] = usedMem;
   cast->values[1] = buffersMem;
   cast->values[2] = cachedMem;
   snprintf(cast->displayBuffer.c, 14, "%ld/%ldMB", usedMem / 1024, this->pl->totalMem / 1024);
}

void MemoryMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   MemoryMeter* this = (MemoryMeter*)cast;
   Meter* meter = (Meter*)cast;
   int div = 1024; char* format = "%ldM ";
   if (meter->w > this->wideLimit) {
      div = 1; format = this->wideFormat;
   }
   long int totalMem = meter->total / div;
   long int usedMem = meter->values[0] / div;
   long int buffersMem = meter->values[1] / div;
   long int cachedMem = meter->values[2] / div;
   RichString_prune(out);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   sprintf(buffer, format, totalMem);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   sprintf(buffer, format, usedMem);
   RichString_append(out, CRT_colors[METER_TEXT], "used:");
   RichString_append(out, CRT_colors[MEMORY_USED], buffer);
   sprintf(buffer, format, buffersMem);
   RichString_append(out, CRT_colors[METER_TEXT], "buffers:");
   RichString_append(out, CRT_colors[MEMORY_BUFFERS], buffer);
   sprintf(buffer, format, cachedMem);
   RichString_append(out, CRT_colors[METER_TEXT], "cache:");
   RichString_append(out, CRT_colors[MEMORY_CACHE], buffer);
}
