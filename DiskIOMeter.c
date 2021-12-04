/*
htop - DiskIOMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "DiskIOMeter.h"

#include <stdbool.h>
#include <stdio.h>

#include "CRT.h"
#include "Macros.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "XUtils.h"


static const int DiskIOMeter_attributes[] = {
   METER_VALUE_NOTICE,
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static MeterRateStatus status = RATESTATUS_INIT;
static uint32_t cached_read_diff;
static uint32_t cached_write_diff;
static double cached_utilisation_diff;

static void DiskIOMeter_updateValues(Meter* this) {
   const ProcessList* pl = this->pl;

   static uint64_t cached_last_update;
   uint64_t passedTimeInMs = pl->realtimeMs - cached_last_update;

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs > 500) {
      static uint64_t cached_read_total;
      static uint64_t cached_write_total;
      static uint64_t cached_msTimeSpend_total;
      uint64_t diff;

      DiskIOData data;
      if (!Platform_getDiskIO(&data)) {
         status = RATESTATUS_NODATA;
      } else if (cached_last_update == 0) {
         status = RATESTATUS_INIT;
      } else if (passedTimeInMs > 30000) {
         status = RATESTATUS_STALE;
      } else {
         status = RATESTATUS_DATA;
      }

      cached_last_update = pl->realtimeMs;

      if (status == RATESTATUS_NODATA) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
         return;
      }

      if (data.totalBytesRead > cached_read_total) {
         diff = data.totalBytesRead - cached_read_total;
         diff /= 1024; /* Meter_humanUnit() expects unit in kilo */
         cached_read_diff = (uint32_t)diff;
      } else {
         cached_read_diff = 0;
      }
      cached_read_total = data.totalBytesRead;

      if (data.totalBytesWritten > cached_write_total) {
         diff = data.totalBytesWritten - cached_write_total;
         diff /= 1024; /* Meter_humanUnit() expects unit in kilo */
         cached_write_diff = (uint32_t)diff;
      } else {
         cached_write_diff = 0;
      }
      cached_write_total = data.totalBytesWritten;

      if (data.totalMsTimeSpend > cached_msTimeSpend_total) {
         diff = data.totalMsTimeSpend - cached_msTimeSpend_total;
         cached_utilisation_diff = 100.0 * (double)diff / passedTimeInMs;
      } else {
         cached_utilisation_diff = 0.0;
      }
      cached_msTimeSpend_total = data.totalMsTimeSpend;
   }

   if (status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
      return;
   }
   if (status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
      return;
   }

   this->values[0] = cached_utilisation_diff;
   this->total = MAXIMUM(this->values[0], 100.0); /* fix total after (initial) spike */

   char bufferRead[12], bufferWrite[12];
   Meter_humanUnit(bufferRead, cached_read_diff, sizeof(bufferRead));
   Meter_humanUnit(bufferWrite, cached_write_diff, sizeof(bufferWrite));
   snprintf(this->txtBuffer, sizeof(this->txtBuffer), "%sB %sB %.1f%%", bufferRead, bufferWrite, cached_utilisation_diff);
}

static void DiskIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   switch (status) {
   case RATESTATUS_NODATA:
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   case RATESTATUS_INIT:
      RichString_writeAscii(out, CRT_colors[METER_VALUE], "initializing...");
      return;
   case RATESTATUS_STALE:
      RichString_writeAscii(out, CRT_colors[METER_VALUE_WARN], "stale data");
      return;
   case RATESTATUS_DATA:
      break;
   }

   char buffer[16];
   int len;

   int color = cached_utilisation_diff > 40.0 ? METER_VALUE_NOTICE : METER_VALUE;
   len = xSnprintf(buffer, sizeof(buffer), "%.1f%%", cached_utilisation_diff);
   RichString_appendnAscii(out, CRT_colors[color], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " read: ");
   Meter_humanUnit(buffer, cached_read_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " write: ");
   Meter_humanUnit(buffer, cached_write_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
}

const MeterClass DiskIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskIOMeter_display
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
