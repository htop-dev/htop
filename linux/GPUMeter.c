/*
htop - GPUMeter.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/GPUMeter.h"

#include "CRT.h"
#include "RichString.h"
#include "XUtils.h"
#include "linux/LinuxMachine.h"


struct EngineData {
   const char* key;  /* owned by LinuxMachine */
   unsigned long long int timeDiff;
   double percentage;
};

static struct EngineData GPUMeter_engineData[4];
static uint64_t prevMonotonicMs;
static double residuePercentage;
static unsigned long long int prevResidueTime;
static double totalUsage;
static unsigned long long int totalGPUTimeDiff;

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
   const Machine* host = this->host;
   const LinuxMachine* lhost = (const LinuxMachine*) host;

   assert(ARRAYSIZE(GPUMeter_engineData) + 1 == ARRAYSIZE(GPUMeter_attributes));

   // The results are cached so that we can update values of multiple meter
   // instances. We also need a local cache of the monotonic timestamp, thus we
   // don't use host->prevMonotonicMs.
   if (host->monotonicMs > prevMonotonicMs) {
      uint64_t monotonictimeDelta = host->monotonicMs - prevMonotonicMs;

      unsigned long long int curResidueTime = lhost->curGpuTime;

      const GPUEngineData* gpuEngineData;
      size_t i;
      for (gpuEngineData = lhost->gpuEngineData, i = 0; gpuEngineData && i < ARRAYSIZE(GPUMeter_engineData); gpuEngineData = gpuEngineData->next, i++) {
         GPUMeter_engineData[i].key        = gpuEngineData->key;
         GPUMeter_engineData[i].timeDiff   = saturatingSub(gpuEngineData->curTime, gpuEngineData->prevTime);
         GPUMeter_engineData[i].percentage = 100.0 * GPUMeter_engineData[i].timeDiff / (1000 * 1000) / monotonictimeDelta;

         curResidueTime = saturatingSub(curResidueTime, gpuEngineData->curTime);
      }

      residuePercentage = 100.0 * saturatingSub(curResidueTime, prevResidueTime) / (1000 * 1000) / monotonictimeDelta;

      totalGPUTimeDiff = saturatingSub(lhost->curGpuTime, lhost->prevGpuTime);
      totalUsage = 100.0 * totalGPUTimeDiff / (1000 * 1000) / monotonictimeDelta;

      prevResidueTime = curResidueTime;
      prevMonotonicMs = host->monotonicMs;
   }

   for (size_t i = 0; i < ARRAYSIZE(GPUMeter_engineData); i++) {
      this->values[i] = GPUMeter_engineData[i].percentage;
   }
   this->values[ARRAYSIZE(GPUMeter_engineData)] = residuePercentage;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", totalUsage);
}

static void GPUMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   int written;
   const Meter* this = (const Meter*)cast;
   unsigned int i;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   written = xSnprintf(buffer, sizeof(buffer), "%4.1f", totalUsage);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
   RichString_appendnAscii(out, CRT_colors[METER_TEXT], "%(", 2);
   written = humanTimeUnit(buffer, sizeof(buffer), totalGPUTimeDiff);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
   RichString_appendnAscii(out, CRT_colors[METER_TEXT], ")", 1);

   for (i = 0; i < ARRAYSIZE(GPUMeter_engineData); i++) {
      if (!GPUMeter_engineData[i].key)
         break;

      RichString_appendnAscii(out, CRT_colors[METER_TEXT], " ", 1);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], GPUMeter_engineData[i].key);
      RichString_appendnAscii(out, CRT_colors[METER_TEXT], ":", 1);
      if (isNonnegative(this->values[i]))
         written = xSnprintf(buffer, sizeof(buffer), "%4.1f", this->values[i]);
      else
         written = xSnprintf(buffer, sizeof(buffer), " N/A");
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
      RichString_appendnAscii(out, CRT_colors[METER_TEXT], "%(", 2);
      written = humanTimeUnit(buffer, sizeof(buffer), GPUMeter_engineData[i].timeDiff);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, written);
      RichString_appendnAscii(out, CRT_colors[METER_TEXT], ")", 1);
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
   .maxItems = ARRAYSIZE(GPUMeter_engineData) + 1,
   .total = 100.0,
   .attributes = GPUMeter_attributes,
   .name = "GPU",
   .uiName = "GPU usage",
   .caption = "GPU"
};
