/*
htop - ZfsArcMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "zfs/ZfsArcMeter.h"

#include <stddef.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"

#include "zfs/ZfsArcStats.h"


static const int ZfsArcMeter_attributes[] = {
   ZFS_MFU, ZFS_MRU, ZFS_ANON, ZFS_HEADER, ZFS_OTHER
};

void ZfsArcMeter_readStats(Meter* this, const ZfsArcStats* stats) {
   this->total = stats->max;
   this->values[0] = stats->MFU;
   this->values[1] = stats->MRU;
   this->values[2] = stats->anon;
   this->values[3] = stats->header;
   this->values[4] = stats->other;

   // "Hide" the last value so it can
   // only be accessed by index and is not
   // displayed by the Bar or Graph style
   this->curItems = 5;
   this->values[5] = stats->size;
}

static void ZfsArcMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;
   Platform_setZfsArcValues(this);

   written = Meter_humanUnit(buffer, this->values[5], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void ZfsArcMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;

   if (this->values[5] > 0) {
      char buffer[50];
      Meter_humanUnit(buffer, this->total, sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
      Meter_humanUnit(buffer, this->values[5], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Used:");
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
      Meter_humanUnit(buffer, this->values[0], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " MFU:");
      RichString_appendAscii(out, CRT_colors[ZFS_MFU], buffer);
      Meter_humanUnit(buffer, this->values[1], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " MRU:");
      RichString_appendAscii(out, CRT_colors[ZFS_MRU], buffer);
      Meter_humanUnit(buffer, this->values[2], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Anon:");
      RichString_appendAscii(out, CRT_colors[ZFS_ANON], buffer);
      Meter_humanUnit(buffer, this->values[3], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Hdr:");
      RichString_appendAscii(out, CRT_colors[ZFS_HEADER], buffer);
      Meter_humanUnit(buffer, this->values[4], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Oth:");
      RichString_appendAscii(out, CRT_colors[ZFS_OTHER], buffer);
   } else {
      RichString_writeAscii(out, CRT_colors[METER_TEXT], " ");
      RichString_appendAscii(out, CRT_colors[FAILED_READ], "Unavailable");
   }
}

const MeterClass ZfsArcMeter_class = {
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
