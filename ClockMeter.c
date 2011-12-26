/*
htop - ClockMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ClockMeter.h"

#include "CRT.h"

#include <time.h>

/*{
#include "Meter.h"
}*/

int ClockMeter_attributes[] = {
   CLOCK
};

static void ClockMeter_setValues(Meter* this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm *lt = localtime(&t);
   this->values[0] = lt->tm_hour * 60 + lt->tm_min;
   strftime(buffer, size, "%H:%M:%S", lt);
}

MeterType ClockMeter = {
   .setValues = ClockMeter_setValues, 
   .display = NULL,
   .mode = TEXT_METERMODE,
   .total = 100.0,
   .items = 1,
   .attributes = ClockMeter_attributes,
   .name = "Clock",
   .uiName = "Clock",
   .caption = "Time: ",
};
