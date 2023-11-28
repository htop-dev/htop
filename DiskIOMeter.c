/*
htop - DiskIOMeter.c
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DiskIOMeter.h"

#include <stdbool.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "Row.h"
#include "XUtils.h"


static const int DiskIOMeter_attributes[] = {
   METER_VALUE_NOTICE,
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static MeterRateStatus status = RATESTATUS_INIT;
static char cached_read_diff_str[6];
static char cached_write_diff_str[6];
static double cached_utilisation_diff;

static void DiskIOMeter_updateValues(Meter* this) {
   const Machine* host = this->host;

   static uint64_t cached_last_update;
   uint64_t passedTimeInMs = host->realtimeMs - cached_last_update;
   bool hasNewData = false;
   DiskIOData data;

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs > 500) {
      hasNewData = Platform_getDiskIO(&data);
      if (!hasNewData) {
         status = RATESTATUS_NODATA;
      } else if (cached_last_update == 0) {
         status = RATESTATUS_INIT;
      } else if (passedTimeInMs > 30000) {
         status = RATESTATUS_STALE;
      } else {
         status = RATESTATUS_DATA;
      }

      cached_last_update = host->realtimeMs;
   }

   if (hasNewData) {
      static uint64_t cached_read_total;
      static uint64_t cached_write_total;
      static uint64_t cached_msTimeSpend_total;

      if (status != RATESTATUS_INIT) {
         uint64_t diff;

         if (data.totalBytesRead > cached_read_total) {
            diff = data.totalBytesRead - cached_read_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
            diff /= ONE_K; /* convert to KiB/s */
         } else {
            diff = 0;
         }
         Meter_humanUnit(cached_read_diff_str, diff, sizeof(cached_read_diff_str));

         if (data.totalBytesWritten > cached_write_total) {
            diff = data.totalBytesWritten - cached_write_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
            diff /= ONE_K; /* convert to KiB/s */
         } else {
            diff = 0;
         }
         Meter_humanUnit(cached_write_diff_str, diff, sizeof(cached_write_diff_str));

         if (data.totalMsTimeSpend > cached_msTimeSpend_total) {
            diff = data.totalMsTimeSpend - cached_msTimeSpend_total;
            cached_utilisation_diff = 100.0 * (double)diff / passedTimeInMs;
            cached_utilisation_diff = MINIMUM(cached_utilisation_diff, 100.0);
         } else {
            cached_utilisation_diff = 0.0;
         }
      }

      cached_read_total = data.totalBytesRead;
      cached_write_total = data.totalBytesWritten;
      cached_msTimeSpend_total = data.totalMsTimeSpend;
   }

   this->values[0] = cached_utilisation_diff;

   if (status == RATESTATUS_NODATA) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
      return;
   }
   if (status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
      return;
   }
   if (status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
      return;
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "r:%siB/s w:%siB/s %.1f%%", cached_read_diff_str, cached_write_diff_str, cached_utilisation_diff);
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

   int color = cached_utilisation_diff > 40.0 ? METER_VALUE_NOTICE : METER_VALUE;
   int len = xSnprintf(buffer, sizeof(buffer), "%.1f%%", cached_utilisation_diff);
   RichString_appendnAscii(out, CRT_colors[color], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " read: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], cached_read_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " write: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], cached_write_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");
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
