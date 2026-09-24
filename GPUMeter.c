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

static int humanTimeUnit(char* buffer, size_t size, unsigned long long totalNanoseconds) {
   if (totalNanoseconds < 10000)
      return xSnprintf(buffer, size, "%4uns", (unsigned int)totalNanoseconds);

   unsigned long long value = totalNanoseconds / 100;

   if (value < 1000)
      return xSnprintf(buffer, size, "%u.%uus", (unsigned int)(value / 10), (unsigned int)(value % 10));

   value /= 10; // microseconds

   if (value < 10000)
      return xSnprintf(buffer, size, "%4uus", (unsigned int)value);

   value /= 100;

   unsigned long long totalSeconds = value / 10000;
   if (totalSeconds < 60) {
      int width = 4;
      unsigned int seconds = (unsigned int)totalSeconds;
      unsigned int fraction = (unsigned int)(value % 10000);
      for (unsigned int limit = 1; seconds >= limit; limit *= 10) {
         width--;
         fraction /= 10;
      }
      // "%.u" prints no digits if (seconds == 0).
      return xSnprintf(buffer, size, "%.u.%0*us", seconds, width, fraction);
   }

   value = totalSeconds;

   if (value < 3600)
      return xSnprintf(buffer, size, "%2um%02us", (unsigned int)value / 60, (unsigned int)value % 60);

   value /= 60; // minutes

   if (value < 1440)
      return xSnprintf(buffer, size, "%2uh%02um", (unsigned int)value / 60, (unsigned int)value % 60);

   value /= 60; // hours

   if (value < 2400)
      return xSnprintf(buffer, size, "%2ud%02uh", (unsigned int)value / 24, (unsigned int)value % 24);

   value /= 24; // days

   if (value < 365)
      return xSnprintf(buffer, size, "%5ud", (unsigned int)value);

   if (value < 3650)
      return xSnprintf(buffer, size, "%uy%03ud", (unsigned int)(value / 365), (unsigned int)(value % 365));

   value /= 365; // years (ignore leap years)

   if (value < 100000)
      return xSnprintf(buffer, size, "%5luy", (unsigned long)value);

   return xSnprintf(buffer, size, "  inf.");
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

   written = xSnprintf(buffer, sizeof(buffer), "%5.1f%%", totalUsage);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
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
      if (isNonnegative(this->values[i])) {
         written = xSnprintf(buffer, sizeof(buffer), "%5.1f%%", this->values[i]);
         RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
      } else {
         RichString_appendAscii(out, CRT_colors[METER_VALUE], " N/A");
      }
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
   .isPercentChart = true,
   .total = 100.0,
   .attributes = GPUMeter_attributes,
   .name = "GPU",
   .uiName = "GPU usage",
   .caption = "GPU"
};
