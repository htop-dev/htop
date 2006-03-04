/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SwapMeter.h"
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

typedef struct SwapMeter_ SwapMeter;

struct SwapMeter_ {
   Meter super;
   ProcessList* pl;
};

}*/

SwapMeter* SwapMeter_new(ProcessList* pl) {
   SwapMeter* this = malloc(sizeof(SwapMeter));
   Meter_init((Meter*)this, String_copy("Swap"), String_copy("Swp"), 1);
   ((Meter*)this)->attributes[0] = SWAP;
   ((Meter*)this)->setValues = SwapMeter_setValues;
   ((Object*)this)->display = SwapMeter_display;
   this->pl = pl;
   Meter_setMode((Meter*)this, BAR);
   return this;
}

void SwapMeter_setValues(Meter* cast) {
   SwapMeter* this = (SwapMeter*)cast;

   double totalSwap = (double)this->pl->totalSwap;
   long int usedSwap = this->pl->usedSwap;
   cast->total = totalSwap;
   cast->values[0] = usedSwap;
   snprintf(cast->displayBuffer.c, 14, "%ld/%ldMB", usedSwap / 1024, this->pl->totalSwap / 1024);
}

void SwapMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* meter = (Meter*)cast;
   long int swap = (long int) meter->values[0];
   RichString_prune(out);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   sprintf(buffer, "%ldM ", (long int) meter->total / 1024);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   sprintf(buffer, "%ldk", swap);
   RichString_append(out, CRT_colors[METER_TEXT], "used:");
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}
