/*
htop - ClockMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ClockMeter.h"

#include <time.h>
#include <sys/time.h>

#include "CRT.h"
#include "Object.h"
#include "ProcessList.h"


static const int ClockMeter_attributes[] = {
   CLOCK
};

static void ClockMeter_updateValues(Meter* this) {
   const ProcessList* pl = this->pl;

   struct tm result;
   const struct tm* lt = localtime_r(&pl->realtime.tv_sec, &result);
   this->values[0] = lt->tm_hour * 60 + lt->tm_min;
   strftime(this->txtBuffer, sizeof(this->txtBuffer), "%H:%M:%S", lt);
}

const MeterClass ClockMeter_class = {
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
