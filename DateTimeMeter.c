/*
htop - DateTimeMeter.c
(C) 2004-2020 Hisham H. Muhammad, Michael Schönitzer
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DateTimeMeter.h"

#include <time.h>
#include <sys/time.h>

#include "CRT.h"
#include "Machine.h"
#include "Object.h"


static const int ClockMeter_attributes[] = {
   CLOCK
};

static const int DateMeter_attributes[] = {
   DATE
};

static const int DateTimeMeter_attributes[] = {
   DATETIME
};

static void DateTimeMeter_updateValues(Meter* this) {
   const Machine* host = this->host;

   struct tm result;
   const struct tm* lt = localtime_r(&host->realtime.tv_sec, &result);
   if (As_Meter(this) == &ClockMeter_class) {
      strftime(this->txtBuffer, sizeof(this->txtBuffer), "%H:%M:%S", lt);
   } else if (As_Meter(this) == &DateMeter_class) {
      strftime(this->txtBuffer, sizeof(this->txtBuffer), "%F", lt);
   } else {
      strftime(this->txtBuffer, sizeof(this->txtBuffer), "%F %H:%M:%S", lt);
   }
}

const MeterClass ClockMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = ClockMeter_attributes,
   .name = "Clock",
   .uiName = "Clock",
   .caption = "Time: ",
};

const MeterClass DateMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = DateMeter_attributes,
   .name = "Date",
   .uiName = "Date",
   .caption = "Date: ",
};

const MeterClass DateTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = DateTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = DateTimeMeter_attributes,
   .name = "DateTime",
   .uiName = "Date and Time",
   .caption = "Date & Time: ",
};
