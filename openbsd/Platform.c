/*
htop - openbsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "SignalsPanel.h"
#include "OpenBSDProcess.h"
#include "OpenBSDProcessList.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/swap.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <math.h>


ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

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

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
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
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

// preserved from FreeBSD port
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
   // this is hard-coded in sys/sys/proc.h - no sysctl exists
   return 99999;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   const OpenBSDProcessList* pl = (OpenBSDProcessList*) this->pl;
   const CPUData* cpuData = &(pl->cpus[cpu]);
   double total = cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod;
   double totalPercent;
   double *v = this->values;

   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->sysPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->intrPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = 0.0;
      v[CPU_METER_STEAL]   = 0.0;
      v[CPU_METER_GUEST]   = 0.0;
      v[CPU_METER_IOWAIT]  = 0.0;
      v[CPU_METER_FREQUENCY] = -1;
      Meter_setItems(this, 8);
      totalPercent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->sysAllPeriod / total * 100.0;
      v[3] = 0.0; // No steal nor guest on OpenBSD
      totalPercent = v[0]+v[1]+v[2];
      Meter_setItems(this, 4);
   }

   totalPercent = CLAMP(totalPercent, 0.0, 100.0);
   if (isnan(totalPercent)) totalPercent = 0.0;
   return totalPercent;
}

void Platform_setMemoryValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   this->total = pl->totalMem;
   this->values[0] = usedMem;
   this->values[1] = buffersMem;
   this->values[2] = cachedMem;
}

/*
 * Copyright (c) 1994 Thorsten Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Taken almost directly from OpenBSD's top(1)
 */
void Platform_setSwapValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   struct swapent *swdev;
   unsigned long long int total, used;
   int nswap, rnswap, i;
   nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap == 0) {
      return;
   }

   swdev = xCalloc(nswap, sizeof(*swdev));

   rnswap = swapctl(SWAP_STATS, swdev, nswap);
   if (rnswap == -1) {
      free(swdev);
      return;
   }

   // if rnswap != nswap, then what?

   /* Total things up */
   total = used = 0;
   for (i = 0; i < nswap; i++) {
      if (swdev[i].se_flags & SWF_ENABLE) {
         used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
         total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
      }
   }

   this->total = pl->totalSwap = total;
   this->values[0] = pl->usedSwap = used;

   free(swdev);
}

void Platform_setTasksValues(Meter* this) {
   // TODO
}

char* Platform_getProcessEnv(pid_t pid) {
   char errbuf[_POSIX2_LINE_MAX];
   char *env;
   char **ptr;
   int count;
   kvm_t *kt;
   struct kinfo_proc *kproc;
   size_t capacity = 4096, size = 0;

   if ((kt = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf)) == NULL)
      return NULL;

   if ((kproc = kvm_getprocs(kt, KERN_PROC_PID, pid,
                             sizeof(struct kinfo_proc), &count)) == NULL) {\
      (void) kvm_close(kt);
      return NULL;
   }

   if ((ptr = kvm_getenvv(kt, kproc, 0)) == NULL) {
      (void) kvm_close(kt);
      return NULL;
   }

   env = xMalloc(capacity);
   for (char **p = ptr; *p; p++) {
      size_t len = strlen(*p) + 1;

      if (size + len > capacity) {
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
       env[size+1] = 0;
   }

   (void) kvm_close(kt);
   return env;
}
