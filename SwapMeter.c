/*
htop
(C) 2004-2011 Hisham H. Muhammad
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

int SwapMeter_attributes[] = {
   SWAP
};

static void SwapMeter_setValues(Meter* this, char* buffer, int len) {
   long int usedSwap = this->pl->usedSwap;
   this->total = this->pl->totalSwap;
   this->values[0] = usedSwap;
   snprintf(buffer, len, "%ld/%ldMB", (long int) usedSwap / 1024, (long int) this->total / 1024);
}

static void SwapMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   long int swap = (long int) this->values[0];
   RichString_write(out, CRT_colors[METER_TEXT], ":");
   sprintf(buffer, "%ldM ", (long int) this->total / 1024);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   sprintf(buffer, "%ldk", swap);
   RichString_append(out, CRT_colors[METER_TEXT], "used:");
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}

MeterType SwapMeter = {
   .setValues = SwapMeter_setValues, 
   .display = SwapMeter_display,
   .mode = BAR_METERMODE,
   .items = 1,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "Swap",
   .uiName = "Swap",
   .caption = "Swp"
};
