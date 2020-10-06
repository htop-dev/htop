/*
htop - DateTimeMeter.c
(C) 2004-2020 Hisham H. Muhammad, Michael Schönitzer
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DateTimeMeter.h"

#include "CRT.h"

#include <time.h>


static const int DateTimeMeter_attributes[] = {
   DATETIME
};

static void DateTimeMeter_updateValues(Meter* this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm result;
   struct tm *lt = localtime_r(&t, &result);
   int year = lt->tm_year + 1900;
   if (((year % 4 == 0) && (year % 100!= 0)) || (year%400 == 0)) {
      this->total = 366;
   }
   else {
      this->total = 365;
   }
   this->values[0] = lt->tm_yday;
   strftime(buffer, size, "%F %H:%M:%S", lt);
}

const MeterClass DateTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 365,
   .attributes = DateTimeMeter_attributes,
   .name = "DateTime",
   .uiName = "Date and Time",
   .caption = "Date & Time: ",
};
