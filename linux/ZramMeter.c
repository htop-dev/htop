#include "ZramMeter.h"

#include "CRT.h"
#include "Meter.h"
#include "Platform.h"

const int ZramMeter_attributes[] = {
   ZRAM
};

static void ZramMeter_updateValues(Meter* this, char* buffer, int size) {
   int written;

   Platform_setZramValues(this);
   written = Meter_humanUnit(buffer, this->values[0], size);

   buffer += written;
   size -= written;
   if(size <= 0) {
      return;
   }
   *buffer++ = '(';
   size--;
   if(size <= 0) {
      return;
   }
   written = Meter_humanUnit(buffer,this->values[1],size);
   buffer += written;
   size -= written;
   if(size <= 0) {
      return;
   }
   *buffer++ = ')';
   size--;
   if ((size -= written) > 0) {
      *buffer++ = '/';
      size--;
      Meter_humanUnit(buffer, this->total, size);
   }
}

static void ZramMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;
   RichString_write(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   Meter_humanUnit(buffer, this->values[0], sizeof(buffer));
   RichString_append(out, CRT_colors[METER_TEXT], " used:");
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}

const MeterClass ZramMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ZramMeter_display,
   },
   .updateValues = ZramMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = ZramMeter_attributes,
   .name = "Zram",
   .uiName = "zram",
   .caption = "zrm"
};
