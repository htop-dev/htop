/*
htop - freebsd/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "freebsd/Platform.h"

#include <devstat.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <sys/_types.h>
#include <sys/devicestat.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vm/vm_param.h>

#include "CPUMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "DiskIOMeter.h"
#include "FileDescriptorMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Machine.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "Meter.h"
#include "NetworkIOMeter.h"
#include "Settings.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "freebsd/FreeBSDMachine.h"
#include "generic/fdstat_sysctl.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsCompressedArcMeter.h"

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

const MeterClass* const Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &MemorySwapMeter_class,
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
   &BlankMeter_class,
   &ZfsArcMeter_class,
   &ZfsCompressedArcMeter_class,
   &DiskIOMeter_class,
   &FileDescriptorMeter_class,
   &NetworkIOMeter_class,
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
   const int mib[2] = { CTL_KERN, KERN_BOOTTIME };
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
   const int mib[2] = { CTL_VM, VM_LOADAVG };
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
      return 99999;
   }
   return maxPid;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const Machine* host = this->host;
   const FreeBSDMachine* fhost = (const FreeBSDMachine*) host;
   unsigned int cpus = host->activeCPUs;

   // single CPU box has everything in fhost->cpus[0]
   const CPUData* cpuData = cpus == 1 ? &fhost->cpus[0] : &fhost->cpus[cpu];

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (host->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      this->curItems = 4;
      percent = v[CPU_METER_NICE] + v[CPU_METER_NORMAL] + v[CPU_METER_KERNEL] + v[CPU_METER_IRQ];
   } else {
      v[CPU_METER_NORMAL] = cpuData->systemAllPercent;
      this->curItems = 3;
      percent = v[CPU_METER_NICE] + v[CPU_METER_NORMAL] + v[CPU_METER_KERNEL];
   }

   percent = CLAMP(percent, 0.0, 100.0);

   v[CPU_METER_FREQUENCY] = cpuData->frequency;
   v[CPU_METER_TEMPERATURE] = cpuData->temperature;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const Machine* host = this->host;
   const FreeBSDMachine* fhost = (const FreeBSDMachine*) host;

   this->total = host->totalMem;
   this->values[MEMORY_METER_USED] = host->usedMem;
   this->values[MEMORY_METER_SHARED] = host->sharedMem;
   // this->values[MEMORY_METER_COMPRESSED] = "compressed memory, like zswap on linux"
   this->values[MEMORY_METER_BUFFERS] = host->buffersMem;
   this->values[MEMORY_METER_CACHE] = host->cachedMem;
   // this->values[MEMORY_METER_AVAILABLE] = "available memory"

   if (fhost->zfs.enabled) {
      // ZFS does not shrink below the value of zfs_arc_min.
      unsigned long long int shrinkableSize = 0;
      if (fhost->zfs.size > fhost->zfs.min)
         shrinkableSize = fhost->zfs.size - fhost->zfs.min;
      this->values[MEMORY_METER_USED] -= shrinkableSize;
      this->values[MEMORY_METER_CACHE] += shrinkableSize;
      // this->values[MEMORY_METER_AVAILABLE] += shrinkableSize;
   }
}

void Platform_setSwapValues(Meter* this) {
   const Machine* host = this->host;

   this->total = host->totalSwap;
   this->values[SWAP_METER_USED] = host->usedSwap;
   // this->values[SWAP_METER_CACHE] = "pages that are both in swap and RAM, like SwapCached on linux"
   // this->values[SWAP_METER_FRONTSWAP] = "pages that are accounted to swap but stored elsewhere, like frontswap on linux"
}

void Platform_setZfsArcValues(Meter* this) {
   const FreeBSDMachine* fhost = (const FreeBSDMachine*) this->host;

   ZfsArcMeter_readStats(this, &fhost->zfs);
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   const FreeBSDMachine* fhost = (const FreeBSDMachine*) this->host;

   ZfsCompressedArcMeter_readStats(this, &fhost->zfs);
}

char* Platform_getProcessEnv(pid_t pid) {
   const int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ENV, pid };

   size_t capacity = ARG_MAX;
   char* env = xMalloc(capacity);

   int err = sysctl(mib, 4, env, &capacity, NULL, 0);
   if (err || capacity == 0) {
      free(env);
      return NULL;
   }

   if (env[capacity - 1] || env[capacity - 2]) {
      env = xRealloc(env, capacity + 2);
      env[capacity] = 0;
      env[capacity + 1] = 0;
   }

   return env;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   (void)pid;
   return NULL;
}

void Platform_getFileDescriptors(double* used, double* max) {
   Generic_getFileDescriptors_sysctl(used, max);
}

bool Platform_getDiskIO(DiskIOData* data) {

   if (devstat_checkversion(NULL) < 0)
      return false;

   // use static to plug memory leak; see #841
   static struct devinfo info = { 0 };
   struct statinfo current = { .dinfo = &info };

   // get number of devices
   if (devstat_getdevs(NULL, &current) < 0)
      return false;

   int count = current.dinfo->numdevs;

   unsigned long long int bytesReadSum = 0, bytesWriteSum = 0, timeSpendSum = 0;

   // get data
   for (int i = 0; i < count; i++) {
      uint64_t bytes_read, bytes_write;
      long double busy_time;

      devstat_compute_statistics(&current.dinfo->devices[i],
                                 NULL,
                                 1.0,
                                 DSM_TOTAL_BYTES_READ, &bytes_read,
                                 DSM_TOTAL_BYTES_WRITE, &bytes_write,
                                 DSM_TOTAL_BUSY_TIME, &busy_time,
                                 DSM_NONE);

      bytesReadSum += bytes_read;
      bytesWriteSum += bytes_write;
      timeSpendSum += 1000 * busy_time;
   }

   data->totalBytesRead = bytesReadSum;
   data->totalBytesWritten = bytesWriteSum;
   data->totalMsTimeSpend = timeSpendSum;
   return true;
}

bool Platform_getNetworkIO(NetworkIOData* data) {
   // get number of interfaces
   int count;
   size_t countLen = sizeof(count);
   const int countMib[] = { CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_SYSTEM, IFMIB_IFCOUNT };

   int r = sysctl(countMib, ARRAYSIZE(countMib), &count, &countLen, NULL, 0);
   if (r < 0)
      return false;

   memset(data, 0, sizeof(NetworkIOData));
   for (int i = 1; i <= count; i++) {
      struct ifmibdata ifmd;
      size_t ifmdLen = sizeof(ifmd);

      const int dataMib[] = { CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL };

      r = sysctl(dataMib, ARRAYSIZE(dataMib), &ifmd, &ifmdLen, NULL, 0);
      if (r < 0)
         continue;

      if (ifmd.ifmd_flags & IFF_LOOPBACK)
         continue;

      data->bytesReceived += ifmd.ifmd_data.ifi_ibytes;
      data->packetsReceived += ifmd.ifmd_data.ifi_ipackets;
      data->bytesTransmitted += ifmd.ifmd_data.ifi_obytes;
      data->packetsTransmitted += ifmd.ifmd_data.ifi_opackets;
   }

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
