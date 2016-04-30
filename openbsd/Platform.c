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

#include <sys/sched.h>
#include <uvm/uvmexp.h>
#include <sys/param.h>
#include <sys/swap.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

}*/

#define MAXCPU 256
// XXX: probably should be a struct member
static int64_t old_v[MAXCPU][5];

/*
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Taken directly from OpenBSD's top(1).
 *
 * percentages(cnt, out, new, old, diffs) - calculate percentage change
 * between array "old" and "new", putting the percentages in "out".
 * "cnt" is size of each array and "diffs" is used for scratch space.
 * The array "old" is updated on each call.
 * The routine assumes modulo arithmetic.  This function is especially
 * useful on BSD machines for calculating cpu state percentages.
 */
static int percentages(int cnt, int64_t *out, int64_t *new, int64_t *old, int64_t *diffs) {
   int64_t change, total_change, *dp, half_total;
   int i;

   /* initialization */
   total_change = 0;
   dp = diffs;

   /* calculate changes for each state and the overall change */
   for (i = 0; i < cnt; i++) {
      if ((change = *new - *old) < 0) {
         /* this only happens when the counter wraps */
         change = INT64_MAX - *old + *new;
      }
      total_change += (*dp++ = change);
      *old++ = *new++;
   }

   /* avoid divide by zero potential */
   if (total_change == 0)
      total_change = 1;

   /* calculate percentages based on overall change, rounding up */
   half_total = total_change / 2l;
   for (i = 0; i < cnt; i++)
      *out++ = ((*diffs++ * 1000 + half_total) / total_change);

   /* return the total in case the caller wants to use it */
   return (total_change);
}

ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

/*
 * See /usr/include/sys/signal.h
 */
SignalItem Platform_signals[] = {
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

unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

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
   return 32766;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   int i;
   double perc;

   OpenBSDProcessList* pl = (OpenBSDProcessList*) this->pl;
   CPUData* cpuData = &(pl->cpus[cpu]);
   int64_t new_v[CPUSTATES], diff_v[CPUSTATES], scratch_v[CPUSTATES];
   double *v = this->values;
   size_t size = sizeof(double) * CPUSTATES;
   int mib[] = { CTL_KERN, KERN_CPTIME2, cpu-1 };
   if (sysctl(mib, 3, new_v, &size, NULL, 0) == -1) {
      return 0.;
   }

   // XXX: why?
   cpuData->totalPeriod = 1;

   percentages(CPUSTATES, diff_v, new_v,
         (int64_t *)old_v[cpu-1], scratch_v);

   for (i = 0; i < CPUSTATES; i++) {
      old_v[cpu-1][i] = new_v[i];
      v[i] = diff_v[i] / 10.;
   }

   Meter_setItems(this, 4);

   perc = v[0] + v[1] + v[2] + v[3];

   if (perc <= 100. && perc >= 0.) {
      return perc;
   } else {
      return 0.;
   }
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
   // TODO
   return NULL;
}
