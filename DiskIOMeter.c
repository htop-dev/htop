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


typedef struct DiskIOMeterData_ {
   Meter* diskIORateMeter;
   Meter* diskIOTimeMeter;
} DiskIOMeterData;

static const int DiskIORateMeter_attributes[] = {
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static const int DiskIOTimeMeter_attributes[] = {
   METER_VALUE_NOTICE,
};

static MeterRateStatus status = RATESTATUS_INIT;
static double cached_read_diff;
static char cached_read_diff_str[6];
static double cached_write_diff;
static char cached_write_diff_str[6];
static uint64_t cached_num_disks;
static double cached_utilisation_diff;
static double cached_utilisation_norm;

static void DiskIOUpdateCache(const Machine* host) {
   static uint64_t cached_last_update;

   uint64_t passedTimeInMs = host->realtimeMs - cached_last_update;

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs <= 500)
      return;

   DiskIOData data;
   bool hasNewData = Platform_getDiskIO(&data);
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

   if (!hasNewData)
      return;

   static uint64_t cached_read_total;
   static uint64_t cached_write_total;
   static uint64_t cached_msTimeSpend_total;

   if (status != RATESTATUS_INIT) {
      uint64_t diff;

      if (data.totalBytesRead > cached_read_total) {
         diff = data.totalBytesRead - cached_read_total;
         diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
      } else {
         diff = 0;
      }
      cached_read_diff = diff;
      Meter_humanUnit(cached_read_diff_str, cached_read_diff / ONE_K, sizeof(cached_read_diff_str));

      if (data.totalBytesWritten > cached_write_total) {
         diff = data.totalBytesWritten - cached_write_total;
         diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
      } else {
         diff = 0;
      }
      cached_write_diff = diff;
      Meter_humanUnit(cached_write_diff_str, cached_write_diff / ONE_K, sizeof(cached_write_diff_str));

      cached_num_disks = data.numDisks;
      cached_utilisation_diff = 0.0;
      cached_utilisation_norm = 0.0;
      if (data.totalMsTimeSpend > cached_msTimeSpend_total) {
         diff = data.totalMsTimeSpend - cached_msTimeSpend_total;
         cached_utilisation_diff = 100.0 * (double)diff / passedTimeInMs;
         if (data.numDisks > 0) {
            cached_utilisation_norm = (double)diff / (passedTimeInMs * data.numDisks);
            cached_utilisation_norm = MINIMUM(cached_utilisation_norm, 1.0);
         }
      }
   }

   cached_read_total = data.totalBytesRead;
   cached_write_total = data.totalBytesWritten;
   cached_msTimeSpend_total = data.totalMsTimeSpend;
}

static void DiskIORateMeter_updateValues(Meter* this) {
   DiskIOUpdateCache(this->host);

   this->values[0] = cached_read_diff;
   this->values[1] = cached_write_diff;

   switch (status) {
      case RATESTATUS_NODATA:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
         return;
      case RATESTATUS_INIT:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
         return;
      case RATESTATUS_STALE:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
         return;
      case RATESTATUS_DATA:
         break;
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "r:%siB/s w:%siB/s", cached_read_diff_str, cached_write_diff_str);
}

static void DiskIORateMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
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

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "read: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], cached_read_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " write: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], cached_write_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");
}

static void DiskIOTimeMeter_updateValues(Meter* this) {
   DiskIOUpdateCache(this->host);

   this->values[0] = cached_utilisation_norm;

   switch (status) {
      case RATESTATUS_NODATA:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
         return;
      case RATESTATUS_INIT:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
         return;
      case RATESTATUS_STALE:
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
         return;
      case RATESTATUS_DATA:
         break;
   }

   char numDisksStr[12];
   numDisksStr[0] = '\0';
   if (cached_num_disks > 1 && cached_num_disks < 1000) {
      xSnprintf(numDisksStr, sizeof(numDisksStr), " (%udisks)", (unsigned int)cached_num_disks);
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%%s", cached_utilisation_diff, numDisksStr);
}

static void DiskIOTimeMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
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
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " busy");

   if (cached_num_disks > 1 && cached_num_disks < 1000) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");
      len = xSnprintf(buffer, sizeof(buffer), "%u", (unsigned int)cached_num_disks);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " disks)");
   }
}

static void DiskIOMeter_display(const Object* cast, RichString* out) {
   DiskIORateMeter_display(cast, out);

   switch (status) {
      case RATESTATUS_NODATA:
      case RATESTATUS_INIT:
      case RATESTATUS_STALE:
         return;
      case RATESTATUS_DATA:
         break;
   }

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "; ");
   DiskIOTimeMeter_display(cast, out);
}

static void DiskIOMeter_updateValues(Meter* this) {
   DiskIOMeterData* data = this->meterData;

   Meter_updateValues(data->diskIORateMeter);
   Meter_updateValues(data->diskIOTimeMeter);
}

static void DiskIOMeter_draw(Meter* this, int x, int y, int w) {
   DiskIOMeterData* data = this->meterData;

   assert(data->diskIORateMeter->draw);
   assert(data->diskIOTimeMeter->draw);

   switch (this->mode) {
   case TEXT_METERMODE:
   case LED_METERMODE:
      data->diskIORateMeter->draw(this, x, y, w);
      return;
   }

   /* Use the same width for each sub meter to align with CPU meter */
   const int colwidth = w / 2;
   const int diff = w % 2;

   data->diskIORateMeter->draw(data->diskIORateMeter, x, y, colwidth);
   data->diskIOTimeMeter->draw(data->diskIOTimeMeter, x + colwidth + diff, y, colwidth);
}

static void DiskIOMeter_init(Meter* this) {
   if (!this->meterData) {
      this->meterData = xCalloc(1, sizeof(DiskIOMeterData));
   }

   DiskIOMeterData* data = this->meterData;

   if (!data->diskIORateMeter)
      data->diskIORateMeter = Meter_new(this->host, 0, (const MeterClass*) Class(DiskIORateMeter));
   if (!data->diskIOTimeMeter)
      data->diskIOTimeMeter = Meter_new(this->host, 0, (const MeterClass*) Class(DiskIOTimeMeter));

   if (Meter_initFn(data->diskIORateMeter)) {
      Meter_init(data->diskIORateMeter);
   }
   if (Meter_initFn(data->diskIOTimeMeter)) {
      Meter_init(data->diskIOTimeMeter);
   }
}

static void DiskIOMeter_updateMode(Meter* this, MeterModeId mode) {
   DiskIOMeterData* data = this->meterData;

   this->mode = mode;

   Meter_setMode(data->diskIORateMeter, mode);
   Meter_setMode(data->diskIOTimeMeter, mode);

   this->h = MAXIMUM(data->diskIORateMeter->h, data->diskIOTimeMeter->h);
}

static void DiskIOMeter_done(Meter* this) {
   DiskIOMeterData* data = this->meterData;

   Meter_delete((Object*)data->diskIORateMeter);
   Meter_delete((Object*)data->diskIOTimeMeter);

   free(data);
}

const MeterClass DiskIORateMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskIORateMeter_display
   },
   .updateValues = DiskIORateMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 2,
   .isPercentChart = false,
   .total = 1.0,
   .attributes = DiskIORateMeter_attributes,
   .name = "DiskIORate",
   .uiName = "Disk IO Rate",
   .description = "Disk IO read & write bytes per second",
   .caption = "Dsk: "
};

const MeterClass DiskIOTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskIOTimeMeter_display
   },
   .updateValues = DiskIOTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .isPercentChart = true,
   .total = 1.0,
   .attributes = DiskIOTimeMeter_attributes,
   .name = "DiskIOTime",
   .uiName = "Disk IO Time",
   .description = "Disk percent time busy",
   .caption = "Dsk: "
};

const MeterClass DiskIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskIOMeter_display
   },
   .updateValues = DiskIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .name = "DiskIO",
   .uiName = "Disk IO",
   .description = "Disk IO rate & time combined display",
   .caption = "Dsk: ",
   .draw = DiskIOMeter_draw,
   .init = DiskIOMeter_init,
   .updateMode = DiskIOMeter_updateMode,
   .done = DiskIOMeter_done
};
