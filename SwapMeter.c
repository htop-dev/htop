/*
htop - SwapMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "SwapMeter.h"

#include <math.h>
#include <stddef.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"


static const int SwapMeter_attributes[] = {
   SWAP,
   SWAP_CACHE,
   SWAP_FRONTSWAP,
};

static void SwapMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;

   this->values[SWAP_METER_CACHE] = NAN;   /* 'cached' not present on all platforms */
   this->values[SWAP_METER_FRONTSWAP] = NAN;   /* 'frontswap' not present on all platforms */
   Platform_setSwapValues(this);

   written = Meter_humanUnit(buffer, this->values[SWAP_METER_USED], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void SwapMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;
   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
   Meter_humanUnit(buffer, this->values[SWAP_METER_USED], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   if (isNonnegative(this->values[SWAP_METER_CACHE])) {
      Meter_humanUnit(buffer, this->values[SWAP_METER_CACHE], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " cache:");
      RichString_appendAscii(out, CRT_colors[SWAP_CACHE], buffer);
   }

   if (isNonnegative(this->values[SWAP_METER_FRONTSWAP])) {
      Meter_humanUnit(buffer, this->values[SWAP_METER_FRONTSWAP], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " frontswap:");
      RichString_appendAscii(out, CRT_colors[SWAP_FRONTSWAP], buffer);
   }
}

const MeterClass SwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SwapMeter_display,
   },
   .updateValues = SwapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = SWAP_METER_ITEMCOUNT,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "Swap",
   .uiName = "Swap",
   .caption = "Swp"
};
