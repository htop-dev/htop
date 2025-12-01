#include "config.h"
#ifdef NVIDIA_JETSON

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "CRT.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Meter.h"
#include "XUtils.h"

#include "linux/LinuxProcess.h"
#include "linux/NvidiaJetson.h"


/*

NVIDIA Jetson devices support is located here.

Jetson has:
- one CPU temperature sensor per 8 cores.
- one GPU temperature sensor, on Jetson Orin it goes off if no GPU load: user gets error on file open
- the process table where kernel nvgpu driver collects GPU clients (experimental, root access only):
    * process id
    * process name
    * memory in kilobytes allocated for GPU (Jetson device shares system RAM for GPU) 

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

If htop starts with root privileges (effective user id is 0), the experimental code activates.
It reads the fixed sysfs file /sys/kernel/debug/nvmap/iovmm/clients with the following content, e.g.:
```
CLIENT                        PROCESS      PID        SIZE
user                         gpu_burn     7979   23525644K
user                      gnome_shell     8119       5800K
user                             Xorg     2651      17876K
total                                            23549320K
```
Unfortunately, the /sys/kernel/debug/... files are allowed to read only for the root user, that's why the restriction applies. 

The patch out of this file adds a separate field 'GPU_MEM', which reads data from LinuxProcess::gpu_mem field.
The field stores memory allocated for GPU in kilobytes. It is populated by the function
NvidiaJetson_LoadGpuProcessTable (the implementation is located here), which is called at the end of the function
Machine_scanTables.

Additionally, the new Action is added: actionToggleGpuFilter, which is activated by 'g' hot key (the help is updated
appropriately). The GpuFilter shows only the processes which currently utilize GPU (i.e. highly extended
nvmap/iovmm/clients table). It is achieved by the filtering machinery associated with ProcessTable::pidMatchList.
The code below constructs GPU_PID_MATCH_LIST hash table, then actionToggleGpuFilter either stores it to the
ProcessTable::pidMatchList or restores old value of ProcessTable::pidMatchList.

The separate LinuxProcess's PROCESS_FLAG_LINUX_GPU_JETSON (or something ...) flag isn't added for GPU_MEM, because
currently the functionality of population LinuxProcess::gpu_mem is shared with the GPU consumers filter construction.
So, even if GPU_MEM field is not activated, the filter showing GPU consumers should work. This kind of architecture is
chosen intentially since it saves memory for the hash table GPU_PID_MATCH_LIST (which is now actually a set), and therefore
increases performance. All other approaches convert GPU_PID_MATCH_LIST to a true key/value storage (key = pid,
value = gpu memory allocated) with further merge code.

*/

/* global paths per each sensor */
char *CpuTempSensorPath = NULL;
char *GpuTempSensorPath = NULL;
char *GpuFreqSensorPath = NULL;
char *GpuLoadSensorPath = NULL;

#define MAX_GPU_PROCESS_COUNT 256UL
static Hashtable *GPU_PID_MATCH_LIST = NULL;

static void NVidiaJetsonHashtableFunctor_ResetGpuMem(ATTR_UNUSED ht_key_t key, void* val, ATTR_UNUSED void* userData) {
   LinuxProcess *lp = val;
   lp->gpu_mem = 0;
}

Hashtable *NvidiaJetson_GetPidMatchList(void) {
   return GPU_PID_MATCH_LIST;
}

void NvidiaJetson_LoadGpuProcessTable(Hashtable *pidHash) {
   static int IsRootUser = -1;

   /* needs root privileges */
   if (!IsRootUser)
      return;

   /* first time function is called */
   if (IsRootUser == -1) {
      IsRootUser = geteuid() == 0;
      if (!IsRootUser)
         return;

      GPU_PID_MATCH_LIST = Hashtable_new(MAX_GPU_PROCESS_COUNT, false);
   }

   FILE *input = fopen("/sys/kernel/debug/nvmap/iovmm/clients", "r");
   if (input == NULL)
      return;

   /* reset all knowledge about GPU allocations */
   Hashtable_foreach(pidHash, NVidiaJetsonHashtableFunctor_ResetGpuMem, NULL);
   Hashtable_clear(GPU_PID_MATCH_LIST);

   /* construct new knowledge regarding GPU allocations */
   static const char sentinel = -128;
   static const size_t line_sz = 256;

   char line[line_sz];
   char *last = &line[sizeof(line) - 1];

   *last = sentinel;
   while (fgets(line, sizeof(line), input)) {
      /* line example: "user   Xorg     2651      17876K" */
      char *saveptr;

      if (String_startsWith(line, "CLIENT")) // skip header
         continue;
      if (String_startsWith(line, "total")) // final line, skip for now
         break;
      if (*last == '\0') {
         /* overflow */
         *last = sentinel;
         continue;
      }
      /* char *user = */ strtok_r(line, " \n", &saveptr);
      /* char *proc = */ strtok_r(NULL, " \n", &saveptr);
      char *pidPtr = strtok_r(NULL, " \n", &saveptr);
      unsigned int pid = fast_strtoull_dec(&pidPtr, 10);

      char *memPtr = strtok_r(NULL, " \n", &saveptr);
      uint64_t gpumem = fast_strtoull_dec(&memPtr, 20);

      /* memory allocation showed in kylobytes, i.e. the token usually looks like "17876K" */
      if (memPtr && *memPtr != 'K') {
         switch (*memPtr) {
            case 'M': gpumem *= 1024; break;
            case 'G': gpumem *= 1024*1024; break;
            default: gpumem = 0; break;
         }
      }

      LinuxProcess *lp = Hashtable_get(pidHash, pid);
      if (lp) {
         lp->gpu_mem = gpumem;
      }

      Hashtable_put(GPU_PID_MATCH_LIST, pid, (void*)1);
   }
   fclose(input);
}

static inline bool IsJetsonOrinGPU(const char *name) {
   return strstr(name, "gpu");
}

static inline bool IsJetsonXavierGPU(const char *name) {
   return strstr(name, "gv11");
}

static void NvidiaJetson_FindGPUDevice(void) {
   const struct dirent* entry;

   #define SYS_CLASS_DEVFREQ "/sys/class/devfreq"

   DIR* dir = opendir(SYS_CLASS_DEVFREQ);
   if (!dir)
      return;

   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
         continue;

      if (IsJetsonOrinGPU(name) || IsJetsonXavierGPU(name)) {
         /* allocated once at the application startup, so not freed until the exit */
         xAsprintf(&GpuFreqSensorPath, SYS_CLASS_DEVFREQ "/%s/cur_freq", name);
         xAsprintf(&GpuLoadSensorPath, SYS_CLASS_DEVFREQ "/%s/device/load", name);
         break;
      }
   }
   closedir(dir);

   #undef SYS_CLASS_DEVFREQ
}

static void NvidiaJetson_GoThroughThermalZones(void) {
   char path[64];
   char content[4];
   const struct dirent* entry;

   #define SYS_DEVICE_VIRTUAL_THERMAL "/sys/devices/virtual/thermal"

   DIR* dir = opendir(SYS_DEVICE_VIRTUAL_THERMAL);
   if (!dir)
      return;

   while ((entry = readdir(dir)) != NULL && (CpuTempSensorPath == NULL || GpuTempSensorPath == NULL)) {
      const char* name = entry->d_name;
      ssize_t ret;

      if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
         continue;

      if (!String_startsWith(name, "thermal_zone"))
         continue;

      xSnprintf(path, sizeof(path), SYS_DEVICE_VIRTUAL_THERMAL "/%s/type", name);
      ret = xReadfile(path, content, sizeof(content));
      if (ret <= 0)
         continue;

      content[0] = tolower(content[0]);
      content[1] = tolower(content[1]);
      content[2] = tolower(content[2]);
      content[3] = tolower(content[3]);

      /* allocated once at the application startup, so not freed until the exit */

      if (CpuTempSensorPath == NULL && String_startsWith(content, "cpu")) {
         xAsprintf(&CpuTempSensorPath, SYS_DEVICE_VIRTUAL_THERMAL "/%s/temp", name);
      }
      if (GpuTempSensorPath == NULL && String_startsWith(content, "gpu")) {
         xAsprintf(&GpuTempSensorPath, SYS_DEVICE_VIRTUAL_THERMAL "/%s/temp", name);
      }
   }
   closedir(dir);

   #undef SYS_DEVICE_VIRTUAL_THERMAL
}

void NvidiaJetson_FindSensors(void) {
   NvidiaJetson_GoThroughThermalZones();
   NvidiaJetson_FindGPUDevice();
}

void NvidiaJetson_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs) {
   char buffer[22];
   double temp = xReadNumberFile(CpuTempSensorPath, buffer, sizeof(buffer)) / 1000.0;
   for (unsigned int i = 0; i <= existingCPUs; ++i)
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
   this->values[JETSON_GPU_LOAD] = xReadNumberFile(GpuLoadSensorPath, buffer, sizeof(buffer));
   this->curItems = 1; /* only show bar for JETSON_GPU_LOAD */

   this->values[JETSON_GPU_TEMP] = xReadNumberFile(GpuTempSensorPath, buffer, sizeof(buffer)) / 1000.0;
   this->values[JETSON_GPU_FREQ] = xReadNumberFile(GpuFreqSensorPath, buffer, sizeof(buffer)) / 1000000.0;
   double percent = this->values[JETSON_GPU_LOAD] / 10.0;

   char c = 'C';
   double gpuTemperature = this->values[JETSON_GPU_TEMP];
   if (this->host->settings->degreeFahrenheit) {
      gpuTemperature = convertCelsiusToFahrenheit(gpuTemperature);
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
