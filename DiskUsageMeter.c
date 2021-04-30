/*
htop - DiskUsageMeter.c
(C) 2021 htop dev team
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DiskUsageMeter.h"

#include <math.h>

#include "CRT.h"
#include "Platform.h"


const DiskUsageData invalidDiskUsageData = { NAN, NAN, NAN };

typedef struct DiskUsageMeterData_ {
   DiskUsageData data;
   char* choice;
   time_t last_updated;
} DiskUsageMeterData;

static const int DiskUsageMeter_attributes[] = {
   METER_VALUE,
};

static void DiskUsageMeter_init(Meter* this) {
   if (!this->meterData)
      this->meterData = xCalloc(1, sizeof(DiskUsageMeterData));
}

static void DiskUsageMeter_done(Meter* this) {
   DiskUsageMeterData* mdata = this->meterData;
   if (!mdata)
      return;

   free(mdata->choice);
   free(mdata);
}

static void DiskUsageMeter_updateValues(Meter* this) {
   DiskUsageMeterData* mdata = this->meterData;

   /* update only every 30s */
   if (this->pl->realtime.tv_sec - mdata->last_updated > 30 || !mdata->choice || (this->curChoice && !String_eq(mdata->choice, this->curChoice))) {
      free(mdata->choice);
      mdata->choice = this->curChoice ? xStrdup(this->curChoice) : NULL;
      mdata->last_updated = this->pl->realtime.tv_sec;

      if (this->curChoice)
         Platform_getDiskUsage(this->curChoice, &mdata->data);
   }

   this->values[0] = isInvalidDiskUsageData(&mdata->data) ? NAN : mdata->data.usedPercentage;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - %.1f%%", this->curChoice ? this->curChoice : "N/A", this->values[0]);
}

static int printBytes(char* buffer, size_t size, double bytes) {
   if (bytes < ONE_DECIMAL_K)
      return xSnprintf(buffer, size, "%.0fB", bytes);
   if (bytes < ONE_DECIMAL_M)
      return xSnprintf(buffer, size, "%.1fKB", bytes / ONE_DECIMAL_K);
   if (bytes < ONE_DECIMAL_G)
      return xSnprintf(buffer, size, "%.1fMB", bytes / ONE_DECIMAL_M);
   if (bytes < ONE_DECIMAL_T)
      return xSnprintf(buffer, size, "%.1fGB", bytes / ONE_DECIMAL_G);
   if (bytes < ONE_DECIMAL_P)
      return xSnprintf(buffer, size, "%.1fTB", bytes / ONE_DECIMAL_T);

   return xSnprintf(buffer, size, "%.0fPB", bytes / ONE_DECIMAL_P);
}

static void DiskUsageMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*) cast;
   DiskUsageMeterData* mdata = this->meterData;

   if (!this->curChoice) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid choice");
      return;
   } else if (isInvalidDiskUsageData(&mdata->data)) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid data for ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], this->curChoice);
      return;
   }

   char buffer[16];
   int len;
   int color = mdata->data.usedPercentage >= 85 ? CRT_colors[METER_VALUE_NOTICE] : CRT_colors[METER_VALUE];

   len = RichString_appendAscii(out, CRT_colors[METER_VALUE], mdata->choice);
   if (len < 15)
      RichString_appendChr(out, CRT_colors[METER_TEXT], ' ', 15 - len);

   len = xSnprintf(buffer, sizeof(buffer), " %4.1f%%", mdata->data.usedPercentage);
   RichString_appendnAscii(out, color, buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");

   len = printBytes(buffer, sizeof(buffer), mdata->data.used);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");

   len = printBytes(buffer, sizeof(buffer), mdata->data.total);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], ")");
}

const MeterClass DiskUsageMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskUsageMeter_display
   },
   .updateValues = DiskUsageMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = DiskUsageMeter_attributes,
   .name = "DiskUsage",
   .uiName = "Disk Usage",
   .caption = "Disk: ",
   .init = DiskUsageMeter_init,
   .done = DiskUsageMeter_done,
   .getChoices = Platform_getDiskUsageChoices,
};
