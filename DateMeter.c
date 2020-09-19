/*
htop - DateMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DateMeter.h"

#include "CRT.h"

#include <time.h>


int DateMeter_attributes[] = {
   DATE
};

static void DateMeter_updateValues(Meter* this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm result;
   struct tm *lt = localtime_r(&t, &result);
   this->values[0] = lt->tm_yday;
   int year = lt->tm_year + 1900;
   if (((year % 4 == 0) && (year % 100!= 0)) || (year%400 == 0)) {
      this->total = 366;
   }
   strftime(buffer, size, "%F", lt);
}

MeterClass DateMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 365,
   .attributes = DateMeter_attributes,
   .name = "Date",
   .uiName = "Date",
   .caption = "Date: ",
};
