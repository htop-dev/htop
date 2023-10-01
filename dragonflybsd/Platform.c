/*
htop - dragonflybsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "dragonflybsd/Platform.h"

#include <math.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vm/vm_param.h>

#include "ClockMeter.h"
#include "CPUMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "FileDescriptorMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "ProcessTable.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "dragonflybsd/DragonFlyBSDProcess.h"
#include "dragonflybsd/DragonFlyBSDProcessTable.h"
#include "generic/fdstat_sysctl.h"


const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Main",
      .columns = "PID USER PRIORITY NICE M_VIRT M_RESIDENT STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

const MeterClass* const Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &MemorySwapMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &SysArchMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &AllCPUs4Meter_class,
   &AllCPUs8Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &LeftCPUs4Meter_class,
   &RightCPUs4Meter_class,
   &LeftCPUs8Meter_class,
   &RightCPUs8Meter_class,
   &FileDescriptorMeter_class,
   &BlankMeter_class,
   NULL
};

bool Platform_init(void) {
   /* no platform-specific setup needed */
   return true;
}

void Platform_done(void) {
   /* no platform-specific cleanup needed */
}

void Platform_setBindings(Htop_Action* keys) {
   /* no platform-specific key bindings */
   (void) keys;
}

int Platform_getUptime(void) {
   struct timeval bootTime, currTime;
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };
   size_t size = sizeof(bootTime);

   int err = sysctl(mib, 2, &bootTime, &size, NULL, 0);
   if (err) {
      return -1;
   }
   gettimeofday(&currTime, NULL);

   return (int) difftime(currTime.tv_sec, bootTime.tv_sec);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   struct loadavg loadAverage;
   int mib[2] = { CTL_VM, VM_LOADAVG };
   size_t size = sizeof(loadAverage);

   int err = sysctl(mib, 2, &loadAverage, &size, NULL, 0);
   if (err) {
      *one = 0;
      *five = 0;
      *fifteen = 0;
      return;
   }
   *one     = (double) loadAverage.ldavg[0] / loadAverage.fscale;
   *five    = (double) loadAverage.ldavg[1] / loadAverage.fscale;
   *fifteen = (double) loadAverage.ldavg[2] / loadAverage.fscale;
}

pid_t Platform_getMaxPid(void) {
   int maxPid;
   size_t size = sizeof(maxPid);
   int err = sysctlbyname("kern.pid_max", &maxPid, &size, NULL, 0);
   if (err) {
      return 999999;
   }
   return maxPid;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const Machine* host = this->host;
   const DragonFlyBSDMachine* dhost = (const DragonFlyBSDMachine*) host;
   unsigned int cpus = host->activeCPUs;
   const CPUData* cpuData;

   if (cpus == 1) {
      // single CPU box has everything in fpl->cpus[0]
      cpuData = &(dhost->cpus[0]);
   } else {
      cpuData = &(dhost->cpus[cpu]);
   }

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (super->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      this->curItems = 4;
   } else {
      v[CPU_METER_KERNEL] = cpuData->systemAllPercent;
      this->curItems = 3;
   }

   percent = sumPositiveValues(v, this->curItems);
   percent = MINIMUM(percent, 100.0);

   v[CPU_METER_FREQUENCY] = NAN;
   v[CPU_METER_TEMPERATURE] = NAN;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const Machine* host = this->host;

   this->total = host->totalMem;
   this->values[MEMORY_METER_USED] = host->usedMem;
   this->values[MEMORY_METER_BUFFERS] = host->buffersMem;
   // this->values[MEMORY_METER_SHARED] = "shared memory, like tmpfs and shm"
   // mtr->values[MEMORY_METER_COMPRESSED] = "compressed memory, like zswap on linux"
   this->values[MEMORY_METER_CACHE] = host->cachedMem;
   // this->values[MEMORY_METER_AVAILABLE] = "available memory"
}

void Platform_setSwapValues(Meter* this) {
   const Machine* host = this->host;
   this->total = host->totalSwap;
   this->values[SWAP_METER_USED] = host->usedSwap;
   // mtr->values[SWAP_METER_CACHE] = "pages that are both in swap and RAM, like SwapCached on linux"
   // mtr->values[SWAP_METER_FRONTSWAP] = "pages that are accounted to swap but stored elsewhere, like frontswap on linux"
}

char* Platform_getProcessEnv(pid_t pid) {
   // TODO
   (void)pid;  // prevent unused warning
   return NULL;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   (void)pid;
   return NULL;
}

void Platform_getFileDescriptors(double* used, double* max) {
   Generic_getFileDescriptors_sysctl(used, max);
}

bool Platform_getDiskIO(DiskIOData* data) {
   // TODO
   (void)data;
   return false;
}

bool Platform_getNetworkIO(NetworkIOData* data) {
   // TODO
   (void)data;
   return false;
}

void Platform_getBattery(double* percent, ACPresence* isOnAC) {
   int life;
   size_t life_len = sizeof(life);
   if (sysctlbyname("hw.acpi.battery.life", &life, &life_len, NULL, 0) == -1)
      *percent = NAN;
   else
      *percent = life;

   int acline;
   size_t acline_len = sizeof(acline);
   if (sysctlbyname("hw.acpi.acline", &acline, &acline_len, NULL, 0) == -1)
      *isOnAC = AC_ERROR;
   else
      *isOnAC = acline == 0 ? AC_ABSENT : AC_PRESENT;
}
