/*
htop - GPUMeter.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "GPUMeter.h"

#include <math.h>

#include "CRT.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


struct GPUMeterEngineData GPUMeter_engineData[4];
static double totalUsage = NAN;
static unsigned long long int totalGPUTimeDiff = -1ULL;

static const int GPUMeter_attributes[] = {
   GPU_ENGINE_1,
   GPU_ENGINE_2,
   GPU_ENGINE_3,
   GPU_ENGINE_4,
   GPU_RESIDUE,
};

static size_t activeMeters;

bool GPUMeter_active(void) {
   return activeMeters > 0;
}

static int humanTimeUnit(char* buffer, size_t size, unsigned long long int value) {

   if (value < 1000)
      return xSnprintf(buffer, size, "%3lluns", value);

   if (value < 10000)
      return xSnprintf(buffer, size, "%1llu.%1lluus", value / 1000, (value % 1000) / 100);

   value /= 1000;

   if (value < 1000)
      return xSnprintf(buffer, size, "%3lluus", value);

   if (value < 10000)
      return xSnprintf(buffer, size, "%1llu.%1llums", value / 1000, (value % 1000) / 100);

   value /= 1000;

   if (value < 1000)
      return xSnprintf(buffer, size, "%3llums", value);

   if (value < 10000)
      return xSnprintf(buffer, size, "%1llu.%1llus", value / 1000, (value % 1000) / 100);

   value /= 1000;

   if (value < 600)
      return xSnprintf(buffer, size, "%3llus", value);

   value /= 60;

   if (value < 600)
      return xSnprintf(buffer, size, "%3llum", value);

   value /= 60;

   if (value < 96)
      return xSnprintf(buffer, size, "%3lluh", value);

   value /= 24;

   return xSnprintf(buffer, size, "%3llud", value);
}

static void GPUMeter_updateValues(Meter* this) {
   assert(ARRAYSIZE(GPUMeter_engineData) <= ARRAYSIZE(GPUMeter_attributes) - 1);

   Platform_setGPUValues(this, &totalUsage, &totalGPUTimeDiff);

   if (!isNonnegative(totalUsage)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      return;
   }
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", totalUsage);
}

static void GPUMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;

   char buffer[50];
   int written;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   if (!isNonnegative(totalUsage)) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE], " N/A");
      return;
   }

   written = xSnprintf(buffer, sizeof(buffer), "%4.1f", totalUsage);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], "%");
   if (totalGPUTimeDiff != -1ULL) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "(");
      written = humanTimeUnit(buffer, sizeof(buffer), totalGPUTimeDiff);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], ")");
   }

   for (size_t i = 0; i < ARRAYSIZE(GPUMeter_engineData); i++) {
      if (!GPUMeter_engineData[i].key)
         break;

      RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], GPUMeter_engineData[i].key);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], ":");
      if (isNonnegative(this->values[i]))
         written = xSnprintf(buffer, sizeof(buffer), "%4.1f", this->values[i]);
      else
         written = xSnprintf(buffer, sizeof(buffer), " N/A");
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "%");
      if (GPUMeter_engineData[i].timeDiff != -1ULL) {
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "(");
         written = humanTimeUnit(buffer, sizeof(buffer), GPUMeter_engineData[i].timeDiff);
         RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], ")");
      }
   }
}

static void GPUMeter_init(Meter* this ATTR_UNUSED) {
   activeMeters++;
}

static void GPUMeter_done(Meter* this ATTR_UNUSED) {
   assert(activeMeters > 0);
   activeMeters--;
}

const MeterClass GPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = GPUMeter_display,
   },
   .init = GPUMeter_init,
   .done = GPUMeter_done,
   .updateValues = GPUMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = ARRAYSIZE(GPUMeter_attributes),
   .total = 100.0,
   .attributes = GPUMeter_attributes,
   .name = "GPU",
   .uiName = "GPU usage",
   .caption = "GPU"
};
