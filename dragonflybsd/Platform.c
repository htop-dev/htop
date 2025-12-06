/*
htop - dragonflybsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "dragonflybsd/Platform.h"

#include <devstat.h>
#include <errno.h>
#include <ifaddrs.h>
#include <math.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/socket.h>
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
#include "dragonflybsd/DragonFlyBSDMachine.h"
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

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number =  1 },
   { .name = " 2 SIGINT",    .number =  2 },
   { .name = " 3 SIGQUIT",   .number =  3 },
   { .name = " 4 SIGILL",    .number =  4 },
   { .name = " 5 SIGTRAP",   .number =  5 },
   { .name = " 6 SIGABRT",   .number =  6 },
   { .name = " 7 SIGEMT",    .number =  7 },
   { .name = " 8 SIGFPE",    .number =  8 },
   { .name = " 9 SIGKILL",   .number =  9 },
   { .name = "10 SIGBUS",    .number = 10 },
   { .name = "11 SIGSEGV",   .number = 11 },
   { .name = "12 SIGSYS",    .number = 12 },
   { .name = "13 SIGPIPE",   .number = 13 },
   { .name = "14 SIGALRM",   .number = 14 },
   { .name = "15 SIGTERM",   .number = 15 },
   { .name = "16 SIGURG",    .number = 16 },
   { .name = "17 SIGSTOP",   .number = 17 },
   { .name = "18 SIGTSTP",   .number = 18 },
   { .name = "19 SIGCONT",   .number = 19 },
   { .name = "20 SIGCHLD",   .number = 20 },
   { .name = "21 SIGTTIN",   .number = 21 },
   { .name = "22 SIGTTOU",   .number = 22 },
   { .name = "23 SIGIO",     .number = 23 },
   { .name = "24 SIGXCPU",   .number = 24 },
   { .name = "25 SIGXFSZ",   .number = 25 },
   { .name = "26 SIGVTALRM", .number = 26 },
   { .name = "27 SIGPROF",   .number = 27 },
   { .name = "28 SIGWINCH",  .number = 28 },
   { .name = "29 SIGINFO",   .number = 29 },
   { .name = "30 SIGUSR1",   .number = 30 },
   { .name = "31 SIGUSR2",   .number = 31 },
   { .name = "32 SIGTHR",    .number = 32 },
   { .name = "33 SIGLIBRT",  .number = 33 },
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

enum {
   MEMORY_CLASS_WIRED = 0,
   MEMORY_CLASS_BUFFERS,
   MEMORY_CLASS_ACTIVE,
   MEMORY_CLASS_CACHE,
   MEMORY_CLASS_INACTIVE,
};

const MemoryClass Platform_memoryClasses[] = {
   { .label = "wired",    .countsAsUsed = true,  .countsAsCache = false, .color = MEMORY_1 },
   { .label = "buffers",  .countsAsUsed = true,  .countsAsCache = false, .color = MEMORY_2 },
   { .label = "active",   .countsAsUsed = true,  .countsAsCache = false, .color = MEMORY_3 },
   { .label = "cache",    .countsAsUsed = false, .countsAsCache = true,  .color = MEMORY_4 },
   { .label = "inactive", .countsAsUsed = false, .countsAsCache = true,  .color = MEMORY_5 },
}; // N.B. the chart will display categories in this order

const unsigned int Platform_numberOfMemoryClasses = ARRAYSIZE(Platform_memoryClasses);


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
   &DiskIORateMeter_class,
   &DiskIOTimeMeter_class,
   &DiskIOMeter_class,
   &NetworkIOMeter_class,
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
   if (host->settings->detailedCPUTime) {
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
   const DragonFlyBSDMachine* fhost = (const DragonFlyBSDMachine*) host;
   const Settings* settings = host->settings;

   this->total = host->totalMem;
   if (settings->showCachedMemory) {
      this->values[MEMORY_CLASS_WIRED]    = fhost->wiredMem;
      this->values[MEMORY_CLASS_BUFFERS]  = fhost->buffersMem;
   }
   else { // if showCachedMemory is disabled, merge buffers into the wired pages
      this->values[MEMORY_CLASS_WIRED]    = fhost->wiredMem + fhost->buffersMem;
      this->values[MEMORY_CLASS_BUFFERS]  = 0;
   }
   this->values[MEMORY_CLASS_ACTIVE]   = fhost->activeMem;
   this->values[MEMORY_CLASS_CACHE]    = fhost->cacheMem;
   this->values[MEMORY_CLASS_INACTIVE] = fhost->inactiveMem;
}

void Platform_setSwapValues(Meter* this) {
   const Machine* host = this->host;
   this->total = host->totalSwap;
   this->values[SWAP_METER_USED] = host->usedSwap;
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
   struct statinfo dev_stats = { 0 };
   struct device_selection* dev_sel = NULL;
   int n_selected, n_selections;
   long sel_gen;

   dev_stats.dinfo = xCalloc(1, sizeof(struct devinfo));

   int ret = getdevs(&dev_stats);
   if (ret < 0) {
      CRT_debug("getdevs() failed [%d]: %s", ret, strerror(errno));
      free(dev_stats.dinfo);
      return false;
   }

   ret = selectdevs(&dev_sel, &n_selected, &n_selections, &sel_gen,
         dev_stats.dinfo->generation, dev_stats.dinfo->devices, dev_stats.dinfo->numdevs,
         NULL, 0, NULL, 0, DS_SELECT_ONLY, dev_stats.dinfo->numdevs, 1);
   if (ret < 0) {
      CRT_debug("selectdevs() failed [%d]: %s", ret, strerror(errno));
      free(dev_stats.dinfo);
      return false;
   }

   uint64_t bytesReadSum = 0;
   uint64_t bytesWriteSum = 0;
   uint64_t busyMsTimeSum = 0;
   uint64_t numDisks = 0;

   for (int i = 0; i < dev_stats.dinfo->numdevs; i++) {
      const struct devstat* device = &dev_stats.dinfo->devices[dev_sel[i].position];

      switch (device->device_type & DEVSTAT_TYPE_MASK) {
      case DEVSTAT_TYPE_DIRECT:
      case DEVSTAT_TYPE_SEQUENTIAL:
      case DEVSTAT_TYPE_WORM:
      case DEVSTAT_TYPE_CDROM:
      case DEVSTAT_TYPE_OPTICAL:
      case DEVSTAT_TYPE_CHANGER:
      case DEVSTAT_TYPE_STORARRAY:
      case DEVSTAT_TYPE_FLOPPY:
         break;
      default:
         continue;
      }

      bytesReadSum  += device->bytes_read;
      bytesWriteSum += device->bytes_written;
      busyMsTimeSum += (device->busy_time.tv_sec * 1000 + device->busy_time.tv_usec / 1000);
      numDisks++;
   }

   data->totalBytesRead = bytesReadSum;
   data->totalBytesWritten = bytesWriteSum;
   data->totalMsTimeSpend = busyMsTimeSum;
   data->numDisks = numDisks;

   free(dev_stats.dinfo);
   return true;
}

bool Platform_getNetworkIO(NetworkIOData* data) {
   struct ifaddrs* ifaddrs = NULL;

   if (getifaddrs(&ifaddrs) != 0)
      return false;

   for (const struct ifaddrs* ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr)
         continue;
      if (ifa->ifa_addr->sa_family != AF_LINK)
         continue;
      if (ifa->ifa_flags & IFF_LOOPBACK)
         continue;

      const struct if_data* ifd = (const struct if_data*)ifa->ifa_data;

      data->bytesReceived += ifd->ifi_ibytes;
      data->packetsReceived += ifd->ifi_ipackets;
      data->bytesTransmitted += ifd->ifi_obytes;
      data->packetsTransmitted += ifd->ifi_opackets;
   }

   freeifaddrs(ifaddrs);
   return true;
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
