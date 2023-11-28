/*
htop - DateMeter.c
(C) 2004-2020 Hisham H. Muhammad, Michael Schönitzer
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DateMeter.h"

#include <time.h>
#include <sys/time.h>

#include "CRT.h"
#include "Machine.h"
#include "Object.h"


static const int DateMeter_attributes[] = {
   DATE
};

static void DateMeter_updateValues(Meter* this) {
   const Machine* host = this->host;

   struct tm result;
   const struct tm* lt = localtime_r(&host->realtime.tv_sec, &result);
   this->values[0] = lt->tm_yday;
   int year = lt->tm_year + 1900;
   if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) {
      this->total = 366;
   } else {
      this->total = 365;
   }
   strftime(this->txtBuffer, sizeof(this->txtBuffer), "%F", lt);
}

const MeterClass DateMeter_class = {
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
