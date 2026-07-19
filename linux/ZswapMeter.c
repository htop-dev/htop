/*
htop - linux/ZswapMeter.c
(C) 2026 Abhiram Shibu
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/ZswapMeter.h"

#include <math.h>
#include <stddef.h>

#include "CRT.h"
#include "Meter.h"
#include "Object.h"
#include "RichString.h"
#include "XUtils.h"
#include "linux/LinuxMachine.h"


static const int ZswapMeter_attributes[ZSWAP_METER_ITEMCOUNT] = {
   [ZSWAP_METER_COMPRESSED] = ZRAM_COMPRESSED,
};

static const int ZswapStatsMeter_attributes[ZSWAP_STATS_METER_ITEMCOUNT] = {
   [ZSWAP_STATS_METER_COMPRESSED] = ZRAM_COMPRESSED,
   [ZSWAP_STATS_METER_ORIGINAL] = ZRAM_UNCOMPRESSED,
};

static int ZswapStatsMeter_printRatio(const Meter* this, char* buffer, size_t size) {
   if (this->values[ZSWAP_STATS_METER_COMPRESSED] > 0) {
      return xSnprintf(buffer, size, "%.2f:1",
         this->values[ZSWAP_STATS_METER_ORIGINAL] / this->values[ZSWAP_STATS_METER_COMPRESSED]);
   }

   return xSnprintf(buffer, size, "N/A");
}

static void ZswapMeter_updateValues(Meter* this) {
   const LinuxMachine* host = (const LinuxMachine*) this->host;
   const ZswapStats* zswap = &host->zswap;

   if (!zswap->available) {
      this->values[ZSWAP_METER_COMPRESSED] = NAN;
      this->total = this->host->totalMem;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "unavailable");
      return;
   }

   this->total = zswap->hasPoolLimit ? zswap->totalZswapPool : this->host->totalMem;

   if (!zswap->enabled) {
      this->values[ZSWAP_METER_COMPRESSED] = NAN;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "disabled");
      return;
   }

   this->values[ZSWAP_METER_COMPRESSED] = zswap->usedZswapComp;

   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written = Meter_humanUnit(buffer, this->values[ZSWAP_METER_COMPRESSED], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   if (zswap->hasPoolLimit)
      Meter_humanUnit(buffer, this->total, size);
   else
      xSnprintf(buffer, size, "?");
}

static void ZswapMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   const LinuxMachine* host = (const LinuxMachine*) this->host;
   const ZswapStats* zswap = &host->zswap;

   if (!zswap->available) {
      RichString_writeAscii(out, CRT_colors[FAILED_READ], "unavailable");
      return;
   }

   if (!zswap->enabled) {
      RichString_writeAscii(out, CRT_colors[METER_SHADOW], "disabled");
      return;
   }

   char buffer[16];
   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");

   if (zswap->hasPoolLimit) {
      Meter_humanUnit(buffer, this->total, sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
   } else {
      RichString_appendAscii(out, CRT_colors[METER_SHADOW], "unknown");
   }

   Meter_humanUnit(buffer, this->values[ZSWAP_METER_COMPRESSED], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   RichString_appendAscii(out, CRT_colors[ZRAM_COMPRESSED], buffer);
}

static void ZswapStatsMeter_updateValues(Meter* this) {
   const LinuxMachine* host = (const LinuxMachine*) this->host;
   const ZswapStats* zswap = &host->zswap;

   if (!zswap->available) {
      this->values[ZSWAP_STATS_METER_COMPRESSED] = NAN;
      this->values[ZSWAP_STATS_METER_ORIGINAL] = NAN;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "unavailable");
      return;
   }

   if (!zswap->enabled) {
      this->values[ZSWAP_STATS_METER_COMPRESSED] = NAN;
      this->values[ZSWAP_STATS_METER_ORIGINAL] = NAN;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "disabled");
      return;
   }

   this->values[ZSWAP_STATS_METER_COMPRESSED] = zswap->usedZswapComp;
   this->values[ZSWAP_STATS_METER_ORIGINAL] = zswap->usedZswapOrig;

   char compressedBuffer[16];
   char originalBuffer[16];
   char ratioBuffer[16];
   Meter_humanUnit(compressedBuffer, this->values[ZSWAP_STATS_METER_COMPRESSED], sizeof(compressedBuffer));
   Meter_humanUnit(originalBuffer, this->values[ZSWAP_STATS_METER_ORIGINAL], sizeof(originalBuffer));
   ZswapStatsMeter_printRatio(this, ratioBuffer, sizeof(ratioBuffer));
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s/%s (%s)", compressedBuffer, originalBuffer, ratioBuffer);
}

static void ZswapStatsMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   const LinuxMachine* host = (const LinuxMachine*) this->host;
   const ZswapStats* zswap = &host->zswap;

   if (!zswap->available) {
      RichString_writeAscii(out, CRT_colors[FAILED_READ], "unavailable");
      return;
   }

   if (!zswap->enabled) {
      RichString_writeAscii(out, CRT_colors[METER_SHADOW], "disabled");
      return;
   }

   char buffer[16];
   Meter_humanUnit(buffer, this->values[ZSWAP_STATS_METER_COMPRESSED], sizeof(buffer));
   RichString_writeAscii(out, CRT_colors[METER_TEXT], "pool:");
   RichString_appendAscii(out, CRT_colors[ZRAM_COMPRESSED], buffer);

   Meter_humanUnit(buffer, this->values[ZSWAP_STATS_METER_ORIGINAL], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " stored:");
   RichString_appendAscii(out, CRT_colors[ZRAM_UNCOMPRESSED], buffer);

   ZswapStatsMeter_printRatio(this, buffer, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " ratio:");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
}

const MeterClass ZswapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZswapMeter_display,
   },
   .updateValues = ZswapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = ZSWAP_METER_ITEMCOUNT,
   .isPercentChart = true,
   .total = 100.0,
   .attributes = ZswapMeter_attributes,
   .name = "Zswap",
   .uiName = "Zswap",
   .caption = "Zsw",
   .description = "Zswap compressed pool usage"
};

const MeterClass ZswapStatsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZswapStatsMeter_display,
   },
   .updateValues = ZswapStatsMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = ZSWAP_STATS_METER_ITEMCOUNT,
   .isPercentChart = false,
   .total = 1.0,
   .attributes = ZswapStatsMeter_attributes,
   .name = "ZswapStats",
   .uiName = "Zswap Stats",
   .caption = "Zsw: ",
   .description = "Zswap compression statistics"
};
