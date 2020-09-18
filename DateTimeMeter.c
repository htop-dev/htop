/*
htop - DateTimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DateTimeMeter.h"

#include "CRT.h"

#include <time.h>


int DateTimeMeter_attributes[] = {
   DATE
};

static void DateTimeMeter_updateValues(Meter* this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm result;
   struct tm *lt = localtime_r(&t, &result);
   this->values[0] = lt->tm_year * 365 * 24 * 60 + lt->tm_yday * 24 * 60 +
                      lt->tm_hour * 60 + lt->tm_min;
   strftime(buffer, size, "%F %H:%M:%S", lt);
}

MeterClass DateTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = DateTimeMeter_attributes,
   .name = "DateTime",
   .uiName = "Date and Time",
   .caption = "Date & Time: ",
};
