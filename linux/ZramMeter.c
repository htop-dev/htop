#include "linux/ZramMeter.h"

#include <stddef.h>

#include "CRT.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "ZramMeter.h"


static const int ZramMeter_attributes[ZRAM_METER_ITEMCOUNT] = {
   [ZRAM_METER_COMPRESSED] = ZRAM_COMPRESSED,
   [ZRAM_METER_UNCOMPRESSED] = ZRAM_UNCOMPRESSED,
};

static void ZramMeter_updateValues(Meter* this) {
   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written;

   Platform_setZramValues(this);

   written = Meter_humanUnit(buffer, this->values[ZRAM_METER_COMPRESSED], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '(');

   written = Meter_humanUnit(buffer, this->values[ZRAM_METER_UNCOMPRESSED], size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, ')');

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void ZramMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");

   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   Meter_humanUnit(buffer, this->values[ZRAM_METER_COMPRESSED], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   Meter_humanUnit(buffer, this->values[ZRAM_METER_UNCOMPRESSED], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " uncompressed:");
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
}

const MeterClass ZramMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZramMeter_display,
   },
   .updateValues = ZramMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = ZRAM_METER_ITEMCOUNT,
   .comprisedValues = true,
   .total = 100.0,
   .attributes = ZramMeter_attributes,
   .name = "Zram",
   .uiName = "Zram",
   .caption = "zrm"
};
