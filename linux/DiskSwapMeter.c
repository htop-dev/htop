/*
htop - DiskSwapMeter.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/DiskSwapMeter.h"

#include <stddef.h>

#include "CRT.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"


static const int DiskSwapMeter_attributes[] = {
   SWAP,
};

static void DiskSwapMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;

   Platform_setDiskSwapValues(this);

   written = Meter_humanUnit(buffer, this->values[0], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void DiskSwapMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");

   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   Meter_humanUnit(buffer, this->values[0], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   RichString_appendAscii(out, CRT_colors[SWAP], buffer);
}

const MeterClass DiskSwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DiskSwapMeter_display,
   },
   .updateValues = DiskSwapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .isPercentChart = true,
   .total = 100.0,
   .attributes = DiskSwapMeter_attributes,
   .name = "DiskSwap",
   .uiName = "Disk Swap",
   .description = "Disk-backed swap usage (not zram)",
   .caption = "Dsw"
};
