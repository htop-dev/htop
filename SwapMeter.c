/*
htop - SwapMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "SwapMeter.h"

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"


static const int SwapMeter_attributes[] = {
   SWAP
};

static void SwapMeter_updateValues(Meter* this, char* buffer, size_t size) {
   int written;
   Platform_setSwapValues(this);

   written = Meter_humanUnit(buffer, this->values[0], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void SwapMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;
   RichString_write(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, 50);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   Meter_humanUnit(buffer, this->values[0], 50);
   RichString_append(out, CRT_colors[METER_TEXT], " used:");
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}

const MeterClass SwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SwapMeter_display,
   },
   .updateValues = SwapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "Swap",
   .uiName = "Swap",
   .caption = "Swp"
};
