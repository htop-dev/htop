/*
htop - ClockMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ClockMeter.h"

#include "CRT.h"

#include <time.h>


int ClockMeter_attributes[] = {
   CLOCK
};

static void ClockMeter_updateValues(Meter* this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm result;
   struct tm *lt = localtime_r(&t, &result);
   this->values[0] = lt->tm_hour * 60 + lt->tm_min;
   strftime(buffer, size, "%H:%M:%S", lt);
}

MeterClass ClockMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = ClockMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 1440, /* 24*60 */
   .attributes = ClockMeter_attributes,
   .name = "Clock",
   .uiName = "Clock",
   .caption = "Time: ",
};
