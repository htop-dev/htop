/*
htop - darwin/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Platform.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include "ClockMeter.h"
#include "CPUMeter.h"
#include "CRT.h"
#include "DarwinProcessList.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "ProcessLocksScreen.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsCompressedArcMeter.h"

#ifdef HAVE_HOST_GET_CLOCK_SERVICE
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#endif


const ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_VIRT, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number =  1 },
   { .name = " 2 SIGINT",    .number =  2 },
   { .name = " 3 SIGQUIT",   .number =  3 },
   { .name = " 4 SIGILL",    .number =  4 },
   { .name = " 5 SIGTRAP",   .number =  5 },
   { .name = " 6 SIGABRT",   .number =  6 },
   { .name = " 6 SIGIOT",    .number =  6 },
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
   &TasksMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &SysArchMeter_class,
   &UptimeMeter_class,
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
   &ZfsArcMeter_class,
   &ZfsCompressedArcMeter_class,
   &BlankMeter_class,
   NULL
};

double Platform_timebaseToNS = 1.0;

long Platform_clockTicksPerSec = -1;

void Platform_init(void) {
   // Check if we can determine the timebase used on this system.
   // If the API is unavailable assume we get our timebase in nanoseconds.
#ifdef HAVE_MACH_TIMEBASE_INFO
   mach_timebase_info_data_t info;
   mach_timebase_info(&info);
   Platform_timebaseToNS = (double)info.numer / (double)info.denom;
#else
   Platform_timebaseToNS = 1.0;
#endif

   // Determine the number of clock ticks per second
   errno = 0;
   Platform_clockTicksPerSec = sysconf(_SC_CLK_TCK);

   if (errno || Platform_clockTicksPerSec < 1) {
      CRT_fatalError("Unable to retrieve clock tick rate");
   }
}

void Platform_done(void) {
   /* no platform-specific cleanup needed */
}

void Platform_setBindings(Htop_Action* keys) {
   /* no platform-specific key bindings */
   (void) keys;
}

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
   double results[3];

   if (3 == getloadavg(results, 3)) {
      *one = results[0];
      *five = results[1];
      *fifteen = results[2];
   } else {
      *one = 0;
      *five = 0;
      *fifteen = 0;
   }
}

int Platform_getMaxPid() {
   /* http://opensource.apple.com/source/xnu/xnu-2782.1.97/bsd/sys/proc_internal.hh */
   return 99999;
}

static double Platform_setCPUAverageValues(Meter* mtr) {
   const ProcessList* dpl = mtr->pl;
   unsigned int cpus = dpl->cpuCount;
   double sumNice = 0.0;
   double sumNormal = 0.0;
   double sumKernel = 0.0;
   double sumPercent = 0.0;
   for (unsigned int i = 1; i <= cpus; i++) {
      sumPercent += Platform_setCPUValues(mtr, i);
      sumNice    += mtr->values[CPU_METER_NICE];
      sumNormal  += mtr->values[CPU_METER_NORMAL];
      sumKernel  += mtr->values[CPU_METER_KERNEL];
   }
   mtr->values[CPU_METER_NICE]   = sumNice   / cpus;
   mtr->values[CPU_METER_NORMAL] = sumNormal / cpus;
   mtr->values[CPU_METER_KERNEL] = sumKernel / cpus;
   return sumPercent / cpus;
}

double Platform_setCPUValues(Meter* mtr, unsigned int cpu) {

   if (cpu == 0) {
      return Platform_setCPUAverageValues(mtr);
   }

   const DarwinProcessList* dpl = (const DarwinProcessList*)mtr->pl;
   const processor_cpu_load_info_t prev = &dpl->prev_load[cpu - 1];
   const processor_cpu_load_info_t curr = &dpl->curr_load[cpu - 1];
   double total = 0;

   /* Take the sums */
   for (size_t i = 0; i < CPU_STATE_MAX; ++i) {
      total += (double)curr->cpu_ticks[i] - (double)prev->cpu_ticks[i];
   }

   mtr->values[CPU_METER_NICE]
      = ((double)curr->cpu_ticks[CPU_STATE_NICE] - (double)prev->cpu_ticks[CPU_STATE_NICE]) * 100.0 / total;
   mtr->values[CPU_METER_NORMAL]
      = ((double)curr->cpu_ticks[CPU_STATE_USER] - (double)prev->cpu_ticks[CPU_STATE_USER]) * 100.0 / total;
   mtr->values[CPU_METER_KERNEL]
      = ((double)curr->cpu_ticks[CPU_STATE_SYSTEM] - (double)prev->cpu_ticks[CPU_STATE_SYSTEM]) * 100.0 / total;

   mtr->curItems = 3;

   /* Convert to percent and return */
   total = mtr->values[CPU_METER_NICE] + mtr->values[CPU_METER_NORMAL] + mtr->values[CPU_METER_KERNEL];

   mtr->values[CPU_METER_FREQUENCY] = NAN;
   mtr->values[CPU_METER_TEMPERATURE] = NAN;

   return CLAMP(total, 0.0, 100.0);
}

void Platform_setMemoryValues(Meter* mtr) {
   const DarwinProcessList* dpl = (const DarwinProcessList*)mtr->pl;
   const struct vm_statistics* vm = &dpl->vm_stats;
   double page_K = (double)vm_page_size / (double)1024;

   mtr->total = dpl->host_info.max_mem / 1024;
   mtr->values[0] = (double)(vm->active_count + vm->wire_count) * page_K;
   mtr->values[1] = (double)vm->purgeable_count * page_K;
   // mtr->values[2] = "shared memory, like tmpfs and shm"
   mtr->values[3] = (double)vm->inactive_count * page_K;
   // mtr->values[4] = "available memory"
}

void Platform_setSwapValues(Meter* mtr) {
   int mib[2] = {CTL_VM, VM_SWAPUSAGE};
   struct xsw_usage swapused;
   size_t swlen = sizeof(swapused);
   sysctl(mib, 2, &swapused, &swlen, NULL, 0);

   mtr->total = swapused.xsu_total / 1024;
   mtr->values[0] = swapused.xsu_used / 1024;
}

void Platform_setZfsArcValues(Meter* this) {
   const DarwinProcessList* dpl = (const DarwinProcessList*) this->pl;

   ZfsArcMeter_readStats(this, &(dpl->zfs));
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   const DarwinProcessList* dpl = (const DarwinProcessList*) this->pl;

   ZfsCompressedArcMeter_readStats(this, &(dpl->zfs));
}

char* Platform_getProcessEnv(pid_t pid) {
   char* env = NULL;

   int argmax;
   size_t bufsz = sizeof(argmax);

   int mib[3];
   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;
   if (sysctl(mib, 2, &argmax, &bufsz, 0, 0) == 0) {
      char* buf = xMalloc(argmax);
      if (buf) {
         mib[0] = CTL_KERN;
         mib[1] = KERN_PROCARGS2;
         mib[2] = pid;
         bufsz = argmax;
         if (sysctl(mib, 3, buf, &bufsz, 0, 0) == 0) {
            if (bufsz > sizeof(int)) {
               char *p = buf, *endp = buf + bufsz;
               int argc = *(int*)(void*)p;
               p += sizeof(int);

               // skip exe
               p = strchr(p, 0) + 1;

               // skip padding
               while (!*p && p < endp)
                  ++p;

               // skip argv
               for (; argc-- && p < endp; p = strrchr(p, 0) + 1)
                  ;

               // skip padding
               while (!*p && p < endp)
                  ++p;

               size_t size = endp - p;
               env = xMalloc(size + 2);
               memcpy(env, p, size);
               env[size] = 0;
               env[size + 1] = 0;
            }
         }
         free(buf);
      }
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
   CFTypeRef power_sources = IOPSCopyPowerSourcesInfo();

   *percent = NAN;
   *isOnAC = AC_ERROR;

   if (NULL == power_sources)
      return;

   CFArrayRef list = IOPSCopyPowerSourcesList(power_sources);
   CFDictionaryRef battery = NULL;
   int len;

   if (NULL == list) {
      CFRelease(power_sources);

      return;
   }

   len = CFArrayGetCount(list);

   /* Get the battery */
   for (int i = 0; i < len && battery == NULL; ++i) {
      CFDictionaryRef candidate = IOPSGetPowerSourceDescription(power_sources,
                                  CFArrayGetValueAtIndex(list, i)); /* GET rule */
      CFStringRef type;

      if (NULL != candidate) {
         type = (CFStringRef) CFDictionaryGetValue(candidate,
                CFSTR(kIOPSTransportTypeKey)); /* GET rule */

         if (kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPSInternalType), 0)) {
            CFRetain(candidate);
            battery = candidate;
         }
      }
   }

   if (NULL != battery) {
      /* Determine the AC state */
      CFStringRef power_state = CFDictionaryGetValue(battery, CFSTR(kIOPSPowerSourceStateKey));

      *isOnAC = (kCFCompareEqualTo == CFStringCompare(power_state, CFSTR(kIOPSACPowerValue), 0))
              ? AC_PRESENT
              : AC_ABSENT;

      /* Get the percentage remaining */
      double current;
      double max;

      CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSCurrentCapacityKey)),
                       kCFNumberDoubleType, &current);
      CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSMaxCapacityKey)),
                       kCFNumberDoubleType, &max);

      *percent = (current * 100.0) / max;

      CFRelease(battery);
   }

   CFRelease(list);
   CFRelease(power_sources);
}

void Platform_gettime_monotonic(uint64_t* msec) {

#ifdef HAVE_HOST_GET_CLOCK_SERVICE

   clock_serv_t cclock;
   mach_timespec_t mts;

   host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);

   *msec = ((uint64_t)mts.tv_sec * 1000) + ((uint64_t)mts.tv_nsec / 1000000);

#else

   Generic_gettime_monotomic(msec);

#endif
}
