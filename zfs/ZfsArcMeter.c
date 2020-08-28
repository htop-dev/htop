/*
htop - ZfsArcMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ZfsArcMeter.h"
#include "ZfsArcStats.h"

#include "CRT.h"
#include "Platform.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <assert.h>

/*{
#include "ZfsArcStats.h"

#include "Meter.h"
}*/

int ZfsArcMeter_attributes[] = {
   ZFS_MFU, ZFS_MRU, ZFS_ANON, ZFS_HEADER, ZFS_OTHER
};

void ZfsArcMeter_readStats(Meter* this, ZfsArcStats* stats) {
   this->total = stats->max;
   this->values[0] = stats->MFU;
   this->values[1] = stats->MRU;
   this->values[2] = stats->anon;
   this->values[3] = stats->header;
   this->values[4] = stats->other;

   // "Hide" the last value so it can
   // only be accessed by index and is not
   // displayed by the Bar or Graph style
   Meter_setItems(this, 5);
   this->values[5] = stats->size;
}

static void ZfsArcMeter_updateValues(Meter* this, char* buffer, int size) {
   int written;
   Platform_setZfsArcValues(this);

   written = Meter_humanUnit(buffer, this->values[5], size);
   buffer += written;
   if ((size -= written) > 0) {
      *buffer++ = '/';
      size--;
      Meter_humanUnit(buffer, this->total, size);
   }
}

static void ZfsArcMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;

   if (this->values[5] > 0) {
      Meter_humanUnit(buffer, this->total, 50);
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      Meter_humanUnit(buffer, this->values[5], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " Used:");
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      Meter_humanUnit(buffer, this->values[0], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " MFU:");
      RichString_append(out, CRT_colors[ZFS_MFU], buffer);
      Meter_humanUnit(buffer, this->values[1], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " MRU:");
      RichString_append(out, CRT_colors[ZFS_MRU], buffer);
      Meter_humanUnit(buffer, this->values[2], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " Anon:");
      RichString_append(out, CRT_colors[ZFS_ANON], buffer);
      Meter_humanUnit(buffer, this->values[3], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " Hdr:");
      RichString_append(out, CRT_colors[ZFS_HEADER], buffer);
      Meter_humanUnit(buffer, this->values[4], 50);
      RichString_append(out, CRT_colors[METER_TEXT], " Oth:");
      RichString_append(out, CRT_colors[ZFS_OTHER], buffer);
   } else {
      RichString_write(out, CRT_colors[METER_TEXT], " ");
      RichString_append(out, CRT_colors[FAILED_SEARCH], "Unavailable");
   }
}

MeterClass ZfsArcMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZfsArcMeter_display,
   },
   .updateValues = ZfsArcMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 6,
   .total = 100.0,
   .attributes = ZfsArcMeter_attributes,
   .name = "ZFSARC",
   .uiName = "ZFS ARC",
   .caption = "ARC: "
};
