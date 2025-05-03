#include "config.h"
#ifdef NVIDIA_JETSON


#include "CRT.h"
#include "Meter.h"
#include "XUtils.h"
#include "linux/NvidiaJetson.h"


#include <ctype.h>
#include <dirent.h>


/*

NVIDIA Jetson devices support is located here.

Jetson has:
- one CPU temperature sensor per 8 cores.
- one GPU temperature sensor, on Jetson Orin it goes off if no GPU load: user gets error on file open

The code tries to find out the correct sensors during the application startup. As an example, the sensors
location for NVIDIA Jetson Orin:
- CPU temperature: /sys/devices/virtual/thermal/thermal_zone0/type
- GPU temperature: /sys/devices/virtual/thermal/thermal_zone1/type
- GPU frequency: /sys/class/devfreq/17000000.gpu/cur_freq
- GPU curr load: /sys/class/devfreq/17000000.gpu/device/load

Measure:
- The GPU frequency is provided in Hz, shown in MHz.
- The CPU/GPU temperatures are provided in Celsius multipled by 1000 (milli Celsius), shown in Cesius
- The Farenheit support is not provided
*/


/* global paths per each sensor */
char CPU_TEMP_SENSOR_PATH[64];
char GPU_TEMP_SENSOR_PATH[64];
char GPU_FREQ_SENSOR_PATH[64];
char GPU_LOAD_SENSOR_PATH[64];


static inline bool IsJetsonOrinGPU(const char *name) {
   return strstr(name, "gpu");
}


static inline bool IsJetsonXavierGPU(const char *name) {
   return strstr(name, "gv11");
}


static void NvidiaJetson_FindGPUDevice(void) {
   const struct dirent* entry;

   DIR* dir = opendir("/sys/class/devfreq");
   if (!dir)
      return;

   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      if (String_eq(name, ".") || String_eq(name, ".."))
         continue;

      if (IsJetsonOrinGPU(name) || IsJetsonXavierGPU(name)) {
         xSnprintf(GPU_FREQ_SENSOR_PATH, sizeof(GPU_FREQ_SENSOR_PATH), "/sys/class/devfreq/%s/cur_freq", name);
         xSnprintf(GPU_LOAD_SENSOR_PATH, sizeof(GPU_LOAD_SENSOR_PATH), "/sys/class/devfreq/%s/device/load", name);
         break;
      }
   }
   closedir(dir);
}


static void NvidiaJetson_GoThroughThermalZones(void) {
   char path[64], content[4];
   const struct dirent* entry;

   DIR* dir = opendir("/sys/devices/virtual/thermal");
   if (!dir)
      return;

   while ((entry = readdir(dir)) != NULL && (CPU_TEMP_SENSOR_PATH[0] == 0 || GPU_TEMP_SENSOR_PATH[0] == 0)) {
      const char* name = entry->d_name;
      ssize_t ret;

      if (String_eq(name, ".") || String_eq(name, ".."))
         continue;

      if (!String_startsWith(name, "thermal_zone"))
         continue;

      xSnprintf(path, sizeof(path), "/sys/devices/virtual/thermal/%s/type", name);
      ret = xReadfile(path, content, sizeof(content));
      if (ret <= 0)
         continue;

      content[0] = tolower(content[0]);
      content[1] = tolower(content[1]);
      content[2] = tolower(content[2]);
      content[3] = tolower(content[3]);

      if (CPU_TEMP_SENSOR_PATH[0] == 0 && String_startsWith(content, "cpu")) {
         xSnprintf(CPU_TEMP_SENSOR_PATH, sizeof(CPU_TEMP_SENSOR_PATH), "/sys/devices/virtual/thermal/%s/temp", name);
      }
      if (GPU_TEMP_SENSOR_PATH[0] == 0 && String_startsWith(content, "gpu")) {
         xSnprintf(GPU_TEMP_SENSOR_PATH, sizeof(GPU_TEMP_SENSOR_PATH), "/sys/devices/virtual/thermal/%s/temp", name);
      }
   }
   closedir(dir);
}


void NvidiaJetson_FindSensors(void) {
   NvidiaJetson_GoThroughThermalZones();
   NvidiaJetson_FindGPUDevice();
}


void NvidiaJetson_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs) {
   char buffer[22];
   double temp = xReadNumberFromFile(CPU_TEMP_SENSOR_PATH, buffer, sizeof(buffer)) / 1000.0;
   for (unsigned int i=0; i<=existingCPUs; ++i)
      cpus[i].temperature = temp;
}


enum JetsonValues {
   JETSON_GPU_LOAD = 0,
   JETSON_GPU_TEMP = 1,
   JETSON_GPU_FREQ = 2,
   JETSON_GPU_TOTAL_COUNT,
};


static void JetsonGPUMeter_updateValues(Meter* this) {
   char buffer[22];
   this->values[JETSON_GPU_LOAD] = xReadNumberFromFile(GPU_LOAD_SENSOR_PATH, buffer, sizeof(buffer));
   this->curItems = 1; /* only show bar for JETSON_GPU_LOAD */

   this->values[JETSON_GPU_TEMP] = xReadNumberFromFile(GPU_TEMP_SENSOR_PATH, buffer, sizeof(buffer)) / 1000.0;
   this->values[JETSON_GPU_FREQ] = xReadNumberFromFile(GPU_FREQ_SENSOR_PATH, buffer, sizeof(buffer)) / 1000000.0;
   double percent = this->values[0] / 10.0;

   char c = 'C';
   double gpuTemperature = this->values[JETSON_GPU_TEMP];
   if (this->host->settings->degreeFahrenheit) {
      gpuTemperature = ConvCelsiusToFahrenheit(gpuTemperature);
      c = 'F';
   }

   unsigned int gpuFrequency = this->values[JETSON_GPU_FREQ];
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%% %3uMHz %.1f%s%c",
      percent, gpuFrequency, gpuTemperature, CRT_degreeSign, c
   );
}

static void JetsonGPUMeter_display(const Object* cast, RichString* out) {
   char buffer[32];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   xSnprintf(buffer, sizeof(buffer), "%.1f", this->values[JETSON_GPU_LOAD]);
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " freq:");
   xSnprintf(buffer, sizeof(buffer), "%3uMHz", (unsigned)this->values[JETSON_GPU_FREQ]);
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " temp:");
   xSnprintf(buffer, sizeof(buffer), "%.1f%sC", this->values[JETSON_GPU_TEMP], CRT_degreeSign);
   RichString_appendWide(out, CRT_colors[METER_VALUE], buffer);
}

static const int JetsonGPUMeter_attributes[] = {
   DEFAULT_COLOR
};

const MeterClass JetsonGPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = JetsonGPUMeter_display,
   },
   .updateValues = JetsonGPUMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = JETSON_GPU_TOTAL_COUNT,
   .total = 1000.0,
   .attributes = JetsonGPUMeter_attributes,
   .name = "jetson_gpu",
   .uiName = "Jetson GPU",
   .caption = "GPU"
};
#endif
