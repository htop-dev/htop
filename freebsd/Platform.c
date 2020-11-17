/*
htop - freebsd/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"

#include <devstat.h>
#include <math.h>
#include <time.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vm/vm_param.h>

#include "CPUMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "DiskIOMeter.h"
#include "FreeBSDProcess.h"
#include "FreeBSDProcessList.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "Meter.h"
#include "NetworkIOMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsCompressedArcMeter.h"


ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

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

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

const MeterClass* const Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
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
   &NetworkIOMeter_class,
   NULL
};

int Platform_getUptime() {
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

int Platform_getMaxPid() {
   int maxPid;
   size_t size = sizeof(maxPid);
   int err = sysctlbyname("kern.pid_max", &maxPid, &size, NULL, 0);
   if (err) {
      return 99999;
   }
   return maxPid;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   const FreeBSDProcessList* fpl = (const FreeBSDProcessList*) this->pl;
   int cpus = this->pl->cpuCount;
   const CPUData* cpuData;

   if (cpus == 1) {
      // single CPU box has everything in fpl->cpus[0]
      cpuData = &(fpl->cpus[0]);
   } else {
      cpuData = &(fpl->cpus[cpu]);
   }

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      this->curItems = 4;
      percent = v[0] + v[1] + v[2] + v[3];
   } else {
      v[2] = cpuData->systemAllPercent;
      this->curItems = 3;
      percent = v[0] + v[1] + v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);

   v[CPU_METER_FREQUENCY] = NAN;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   // TODO
   const ProcessList* pl = this->pl;

   this->total = pl->totalMem;
   this->values[0] = pl->usedMem;
   this->values[1] = pl->buffersMem;
   this->values[2] = pl->cachedMem;
}

void Platform_setSwapValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

void Platform_setZfsArcValues(Meter* this) {
   const FreeBSDProcessList* fpl = (const FreeBSDProcessList*) this->pl;

   ZfsArcMeter_readStats(this, &(fpl->zfs));
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   const FreeBSDProcessList* fpl = (const FreeBSDProcessList*) this->pl;

   ZfsCompressedArcMeter_readStats(this, &(fpl->zfs));
}

char* Platform_getProcessEnv(pid_t pid) {
   int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ENV, pid };

   size_t capacity = ARG_MAX;
   char* env = xMalloc(capacity);

   int err = sysctl(mib, 4, env, &capacity, NULL, 0);
   if (err) {
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

char* Platform_getInodeFilename(pid_t pid, ino_t inode) {
    (void)pid;
    (void)inode;
    return NULL;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
    (void)pid;
    return NULL;
}

bool Platform_getDiskIO(DiskIOData* data) {

   if (devstat_checkversion(NULL) < 0)
      return false;

   struct devinfo info = { 0 };
   struct statinfo current = { .dinfo = &info };

   // get number of devices
   if (devstat_getdevs(NULL, &current) < 0)
      return false;

   int count = current.dinfo->numdevs;

   unsigned long int bytesReadSum = 0, bytesWriteSum = 0, timeSpendSum = 0;

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

bool Platform_getNetworkIO(unsigned long int* bytesReceived,
                           unsigned long int* packetsReceived,
                           unsigned long int* bytesTransmitted,
                           unsigned long int* packetsTransmitted) {
   int r;

   // get number of interfaces
   int count;
   size_t countLen = sizeof(count);
   const int countMib[] = { CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_SYSTEM, IFMIB_IFCOUNT };

   r = sysctl(countMib, ARRAYSIZE(countMib), &count, &countLen, NULL, 0);
   if (r < 0)
      return false;


   unsigned long int bytesReceivedSum = 0, packetsReceivedSum = 0, bytesTransmittedSum = 0, packetsTransmittedSum = 0;

   for (int i = 1; i <= count; i++) {
      struct ifmibdata ifmd;
      size_t ifmdLen = sizeof(ifmd);

      const int dataMib[] = { CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL };

      r = sysctl(dataMib, ARRAYSIZE(dataMib), &ifmd, &ifmdLen, NULL, 0);
      if (r < 0)
         continue;

      if (ifmd.ifmd_flags & IFF_LOOPBACK)
         continue;

      bytesReceivedSum += ifmd.ifmd_data.ifi_ibytes;
      packetsReceivedSum += ifmd.ifmd_data.ifi_ipackets;
      bytesTransmittedSum += ifmd.ifmd_data.ifi_obytes;
      packetsTransmittedSum += ifmd.ifmd_data.ifi_opackets;
   }

   *bytesReceived = bytesReceivedSum;
   *packetsReceived = packetsReceivedSum;
   *bytesTransmitted = bytesTransmittedSum;
   *packetsTransmitted = packetsTransmittedSum;
   return true;
}

void Platform_getBattery(double* level, ACPresence* isOnAC) {
   int life;
   size_t life_len = sizeof(life);
   if (sysctlbyname("hw.acpi.battery.life", &life, &life_len, NULL, 0) == -1)
      *level = NAN;
   else
      *level = life;

   int acline;
   size_t acline_len = sizeof(acline);
   if (sysctlbyname("hw.acpi.acline", &acline, &acline_len, NULL, 0) == -1)
      *isOnAC = AC_ERROR;
   else
      *isOnAC = acline == 0 ? AC_ABSENT : AC_PRESENT;
}
