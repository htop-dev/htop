/*
htop - ZfsCompressedArcMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ZfsCompressedArcMeter.h"
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

int ZfsCompressedArcMeter_attributes[] = {
   ZFS_COMPRESSED
};

void ZfsCompressedArcMeter_readStats(Meter* this, ZfsArcStats* stats) {
   if ( stats->isCompressed ) {
      this->total = stats->uncompressed;
      this->values[0] = stats->compressed;
   } else {
      // For uncompressed ARC, report 1:1 ratio
      this->total = stats->size;
      this->values[0] = stats->size;
   }
}

static void ZfsCompressedArcMeter_printRatioString(Meter* this, char* buffer, int size) {
   xSnprintf(buffer, size, "%.2f:1", this->total/this->values[0]);
}

static void ZfsCompressedArcMeter_updateValues(Meter* this, char* buffer, int size) {
   Platform_setZfsCompressedArcValues(this);

   ZfsCompressedArcMeter_printRatioString(this, buffer, size);
}

static void ZfsCompressedArcMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;

   if (this->values[0] > 0) {
      Meter_humanUnit(buffer, this->total, 50);
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      RichString_append(out, CRT_colors[METER_TEXT], " Uncompressed, ");
      Meter_humanUnit(buffer, this->values[0], 50);
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      RichString_append(out, CRT_colors[METER_TEXT], " Compressed, ");
      ZfsCompressedArcMeter_printRatioString(this, buffer, 50);
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      RichString_append(out, CRT_colors[METER_TEXT], " Ratio");
   } else {
      RichString_write(out, CRT_colors[METER_TEXT], " ");
      RichString_append(out, CRT_colors[FAILED_SEARCH], "Compression Unavailable");
   }
}

MeterClass ZfsCompressedArcMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZfsCompressedArcMeter_display,
   },
   .updateValues = ZfsCompressedArcMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = ZfsCompressedArcMeter_attributes,
   .name = "ZFSCARC",
   .uiName = "ZFS CARC",
   .description = "ZFS CARC: Compressed ARC statistics",
   .caption = "ARC: "
};
