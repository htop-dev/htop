/*
htop - DiskIOMeter.c
(C) 2020 Christian Göttsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "DiskIOMeter.h"

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

static unsigned long int cached_read_diff = 0;
static unsigned long int cached_write_diff = 0;
static double cached_utilisation_diff = 0.0;

static void DiskIOMeter_updateValues(Meter* this, char* buffer, int len) {
   static unsigned long int cached_read_total = 0;
   static unsigned long int cached_write_total = 0;
   static unsigned long int cached_msTimeSpend_total = 0;
   static unsigned long long int cached_last_update = 0;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   unsigned long long int timeInMilliSeconds = (unsigned long long int)tv.tv_sec * 1000 + (unsigned long long int)tv.tv_usec / 1000;

   /* update only every 500ms */
   if (timeInMilliSeconds - cached_last_update > 500) {
      unsigned long int bytesRead, bytesWrite, msTimeSpend;

      Platform_getDiskIO(&bytesRead, &bytesWrite, &msTimeSpend);

      cached_read_diff = (bytesRead - cached_read_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
      cached_read_total = bytesRead;

      cached_write_diff = (bytesWrite - cached_write_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
      cached_write_total = bytesWrite;

      cached_utilisation_diff = 100 * (double)(msTimeSpend - cached_msTimeSpend_total) / (timeInMilliSeconds - cached_last_update);
      cached_last_update = timeInMilliSeconds;
      cached_msTimeSpend_total = msTimeSpend;
   }

   this->values[0] = cached_utilisation_diff;
   this->total = MAXIMUM(this->values[0], 100.0); /* fix total after (initial) spike */

   char bufferRead[12], bufferWrite[12];
   Meter_humanUnit(bufferRead, cached_read_diff, sizeof(bufferRead));
   Meter_humanUnit(bufferWrite, cached_write_diff, sizeof(bufferWrite));
   snprintf(buffer, len, "%sB %sB %.1f%%", bufferRead, bufferWrite, cached_utilisation_diff);
}

static void DIskIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
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
