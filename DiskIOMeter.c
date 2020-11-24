/*
htop - DiskIOMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "DiskIOMeter.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int DiskIOMeter_attributes[] = {
   METER_VALUE_NOTICE,
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static bool hasData = false;
static unsigned long int cached_read_diff = 0;
static unsigned long int cached_write_diff = 0;
static double cached_utilisation_diff = 0.0;

static void DiskIOMeter_updateValues(Meter* this, char* buffer, size_t len) {
   static unsigned long long int cached_last_update = 0;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   unsigned long long int timeInMilliSeconds = (unsigned long long int)tv.tv_sec * 1000 + (unsigned long long int)tv.tv_usec / 1000;
   unsigned long long int passedTimeInMs = timeInMilliSeconds - cached_last_update;

   /* update only every 500ms */
   if (passedTimeInMs > 500) {
      static unsigned long int cached_read_total = 0;
      static unsigned long int cached_write_total = 0;
      static unsigned long int cached_msTimeSpend_total = 0;

      cached_last_update = timeInMilliSeconds;

      DiskIOData data;

      hasData = Platform_getDiskIO(&data);
      if (!hasData) {
         this->values[0] = 0;
         xSnprintf(buffer, len, "no data");
         return;
      }

      if (data.totalBytesRead > cached_read_total) {
         cached_read_diff = (data.totalBytesRead - cached_read_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
      } else {
         cached_read_diff = 0;
      }
      cached_read_total = data.totalBytesRead;

      if (data.totalBytesWritten > cached_write_total) {
         cached_write_diff = (data.totalBytesWritten - cached_write_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
      } else {
         cached_write_diff = 0;
      }
      cached_write_total = data.totalBytesWritten;

      if (data.totalMsTimeSpend > cached_msTimeSpend_total) {
         cached_utilisation_diff = 100 * (double)(data.totalMsTimeSpend - cached_msTimeSpend_total) / passedTimeInMs;
      } else {
         cached_utilisation_diff = 0.0;
      }
      cached_msTimeSpend_total = data.totalMsTimeSpend;
   }

   this->values[0] = cached_utilisation_diff;
   this->total = MAXIMUM(this->values[0], 100.0); /* fix total after (initial) spike */

   char bufferRead[12], bufferWrite[12];
   Meter_humanUnit(bufferRead, cached_read_diff, sizeof(bufferRead));
   Meter_humanUnit(bufferWrite, cached_write_diff, sizeof(bufferWrite));
   snprintf(buffer, len, "%sB %sB %.1f%%", bufferRead, bufferWrite, cached_utilisation_diff);
}

static void DIskIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   if (!hasData) {
      RichString_write(out, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   }

   char buffer[16];

   int color = cached_utilisation_diff > 40.0 ? METER_VALUE_NOTICE : METER_VALUE;
   xSnprintf(buffer, sizeof(buffer), "%.1f%%", cached_utilisation_diff);
   RichString_write(out, CRT_colors[color], buffer);

   RichString_append(out, CRT_colors[METER_TEXT], " read: ");
   Meter_humanUnit(buffer, cached_read_diff, sizeof(buffer));
   RichString_append(out, CRT_colors[METER_VALUE_IOREAD], buffer);

   RichString_append(out, CRT_colors[METER_TEXT], " write: ");
   Meter_humanUnit(buffer, cached_write_diff, sizeof(buffer));
   RichString_append(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
}

const MeterClass DiskIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DIskIOMeter_display
   },
   .updateValues = DiskIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = DiskIOMeter_attributes,
   .name = "DiskIO",
   .uiName = "Disk IO",
   .caption = "Disk IO: "
};
