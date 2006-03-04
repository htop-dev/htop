/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ClockMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include <curses.h>
#include <time.h>

#include "debug.h"

/*{

typedef struct ClockMeter_ ClockMeter;

struct ClockMeter_ {
   Meter super;
   ProcessList* pl;
   char clock[10];
};

}*/

ClockMeter* ClockMeter_new() {
   ClockMeter* this = malloc(sizeof(ClockMeter));
   Meter_init((Meter*)this, String_copy("Clock"), String_copy("Time: "), 1);
   ((Meter*)this)->attributes[0] = CLOCK;
   ((Meter*)this)->setValues = ClockMeter_setValues;
   ((Object*)this)->display = ClockMeter_display;
   ((Meter*)this)->total = 24 * 60;
   Meter_setMode((Meter*)this, TEXT);
   return this;
}

void ClockMeter_setValues(Meter* cast) {
   ClockMeter* this = (ClockMeter*) cast;
   time_t t = time(NULL);
   struct tm *lt = localtime(&t);
   cast->values[0] = lt->tm_hour * 60 + lt->tm_min;
   strftime(this->clock, 9, "%H:%M:%S", lt);
   snprintf(cast->displayBuffer.c, 9, "%s", this->clock);
}

void ClockMeter_display(Object* cast, RichString* out) {
   Meter* super = (Meter*) cast;
   ClockMeter* this = (ClockMeter*) cast;
   RichString_write(out, CRT_colors[super->attributes[0]], this->clock);
}
