/*
htop - openbsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "openbsd/Platform.h"

#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/signal.h>  // needs to be included before <sys/proc.h> for 'struct sigaltstack'
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <uvm/uvmexp.h>

#include "CPUMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "Meter.h"
#include "ProcessList.h"
#include "Settings.h"
#include "SignalsPanel.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "openbsd/OpenBSDProcess.h"
#include "openbsd/OpenBSDProcessList.h"


const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Main",
      .columns = "PID USER PRIORITY NICE M_VIRT M_RESIDENT STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

/*
 * See /usr/include/sys/signal.h
 */
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
   { .name = "32 SIGTHR",    .number = 32 },
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

int Platform_getUptime() {
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

int Platform_getMaxPid() {
   return 2 * THREAD_PID_OFFSET;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const OpenBSDProcessList* pl = (const OpenBSDProcessList*) this->pl;
   const CPUData* cpuData = &(pl->cpuData[cpu]);
   double total;
   double totalPercent;
   double* v = this->values;

   if (!cpuData->online) {
      this->curItems = 0;
      return NAN;
   }

   total = cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod;

   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->sysPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->intrPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = 0.0;
      v[CPU_METER_STEAL]   = 0.0;
      v[CPU_METER_GUEST]   = 0.0;
      v[CPU_METER_IOWAIT]  = 0.0;
      v[CPU_METER_FREQUENCY] = NAN;
      this->curItems = 8;
      totalPercent = v[0] + v[1] + v[2] + v[3];
   } else {
      v[2] = cpuData->sysAllPeriod / total * 100.0;
      v[3] = 0.0; // No steal nor guest on OpenBSD
      totalPercent = v[0] + v[1] + v[2];
      this->curItems = 4;
   }

   totalPercent = CLAMP(totalPercent, 0.0, 100.0);

   v[CPU_METER_TEMPERATURE] = NAN;

   v[CPU_METER_FREQUENCY] = (pl->cpuSpeed != -1) ? pl->cpuSpeed : NAN;

   return totalPercent;
}

void Platform_setMemoryValues(Meter* this) {
   const ProcessList* pl = this->pl;
   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   this->total = pl->totalMem;
   this->values[0] = usedMem;
   this->values[1] = buffersMem;
   // this->values[2] = "shared memory, like tmpfs and shm"
   this->values[3] = cachedMem;
   // this->values[4] = "available memory"
}

void Platform_setSwapValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
   this->values[1] = NAN;
}

char* Platform_getProcessEnv(pid_t pid) {
   char errbuf[_POSIX2_LINE_MAX];
   char* env;
   char** ptr;
   int count;
   kvm_t* kt;
   struct kinfo_proc* kproc;
   size_t capacity = 4096, size = 0;

   if ((kt = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf)) == NULL) {
      return NULL;
   }

   if ((kproc = kvm_getprocs(kt, KERN_PROC_PID, pid,
                             sizeof(struct kinfo_proc), &count)) == NULL) {
      (void) kvm_close(kt);
      return NULL;
   }

   if ((ptr = kvm_getenvv(kt, kproc, 0)) == NULL) {
      (void) kvm_close(kt);
      return NULL;
   }

   env = xMalloc(capacity);
   for (char** p = ptr; *p; p++) {
      size_t len = strlen(*p) + 1;

      while (size + len > capacity) {
         if (capacity > (SIZE_MAX / 2)) {
            free(env);
            env = NULL;
            goto end;
         }

         capacity *= 2;
         env = xRealloc(env, capacity);
      }

      strlcpy(env + size, *p, len);
      size += len;
   }

   if (size < 2 || env[size - 1] || env[size - 2]) {
      if (size + 2 < capacity)
         env = xRealloc(env, capacity + 2);
      env[size] = 0;
      env[size + 1] = 0;
   }

end:
   (void) kvm_close(kt);
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

static bool findDevice(const char* name, int* mib, struct sensordev* snsrdev, size_t* sdlen) {
   for (int devn = 0;; devn++) {
      mib[2] = devn;
      if (sysctl(mib, 3, snsrdev, sdlen, NULL, 0) == -1) {
         if (errno == ENXIO)
            continue;
         if (errno == ENOENT)
            return false;
      }
      if (String_eq(name, snsrdev->xname)) {
         return true;
      }
   }
}

void Platform_getBattery(double* percent, ACPresence* isOnAC) {
   int mib[] = {CTL_HW, HW_SENSORS, 0, 0, 0};
   struct sensor s;
   size_t slen = sizeof(struct sensor);
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);

   bool found = findDevice("acpibat0", mib, &snsrdev, &sdlen);

   *percent = NAN;
   if (found) {
      /* last full capacity */
      mib[3] = 7;
      mib[4] = 0;
      double last_full_capacity = 0;
      if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1)
         last_full_capacity = s.value;
      if (last_full_capacity > 0) {
         /*  remaining capacity */
         mib[3] = 7;
         mib[4] = 3;
         if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
            double charge = s.value;
            *percent = 100 * (charge / last_full_capacity);
            if (charge >= last_full_capacity) {
               *percent = 100;
            }
         }
      }
   }

   found = findDevice("acpiac0", mib, &snsrdev, &sdlen);

   *isOnAC = AC_ERROR;
   if (found) {
      mib[3] = 9;
      mib[4] = 0;
      if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1)
         *isOnAC = s.value;
   }
}
