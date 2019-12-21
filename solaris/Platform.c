/*
htop - solaris/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2017,2018 Guy M. Broome
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
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsCompressedArcMeter.h"
#include "SolarisProcess.h"
#include "SolarisProcessList.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <utmpx.h>
#include <sys/loadavg.h>
#include <string.h>
#include <kstat.h>
#include <time.h>
#include <math.h>
#include <sys/var.h>


double plat_loadavg[3] = {0};

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",      .number =  0 },
   { .name = " 1 SIGHUP",      .number =  1 },
   { .name = " 2 SIGINT",      .number =  2 },
   { .name = " 3 SIGQUIT",     .number =  3 },
   { .name = " 4 SIGILL",      .number =  4 },
   { .name = " 5 SIGTRAP",     .number =  5 },
   { .name = " 6 SIGABRT/IOT", .number =  6 },
   { .name = " 7 SIGEMT",      .number =  7 },
   { .name = " 8 SIGFPE",      .number =  8 },
   { .name = " 9 SIGKILL",     .number =  9 },
   { .name = "10 SIGBUS",      .number = 10 },
   { .name = "11 SIGSEGV",     .number = 11 },
   { .name = "12 SIGSYS",      .number = 12 },
   { .name = "13 SIGPIPE",     .number = 13 },
   { .name = "14 SIGALRM",     .number = 14 },
   { .name = "15 SIGTERM",     .number = 15 },
   { .name = "16 SIGUSR1",     .number = 16 },
   { .name = "17 SIGUSR2",     .number = 17 },
   { .name = "18 SIGCHLD/CLD", .number = 18 },
   { .name = "19 SIGPWR",      .number = 19 },
   { .name = "20 SIGWINCH",    .number = 20 },
   { .name = "21 SIGURG",      .number = 21 },
   { .name = "22 SIGPOLL/IO",  .number = 22 },
   { .name = "23 SIGSTOP",     .number = 23 },
   { .name = "24 SIGTSTP",     .number = 24 },
   { .name = "25 SIGCONT",     .number = 25 },
   { .name = "26 SIGTTIN",     .number = 26 },
   { .name = "27 SIGTTOU",     .number = 27 },
   { .name = "28 SIGVTALRM",   .number = 28 },
   { .name = "29 SIGPROF",     .number = 29 },
   { .name = "30 SIGXCPU",     .number = 30 },
   { .name = "31 SIGXFSZ",     .number = 31 },
   { .name = "32 SIGWAITING",  .number = 32 },
   { .name = "33 SIGLWP",      .number = 33 },
   { .name = "34 SIGFREEZE",   .number = 34 },
   { .name = "35 SIGTHAW",     .number = 35 },
   { .name = "36 SIGCANCEL",   .number = 36 },
   { .name = "37 SIGLOST",     .number = 37 },
   { .name = "38 SIGXRES",     .number = 38 },
   { .name = "39 SIGJVM1",     .number = 39 },
   { .name = "40 SIGJVM2",     .number = 40 },
   { .name = "41 SIGINFO",     .number = 41 },
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { PID, LWPID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &UptimeMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &ZfsArcMeter_class,
   &ZfsCompressedArcMeter_class,
   &BlankMeter_class,
   NULL
};

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_numberOfFields = LAST_PROCESSFIELD;

extern char Process_pidFormat[20];

int Platform_getUptime() {
   int boot_time = 0;
   int curr_time = time(NULL);
   struct utmpx * ent;

   while (( ent = getutxent() )) {
      if ( !strcmp("system boot", ent->ut_line )) {
         boot_time = ent->ut_tv.tv_sec;
      }
   }

   endutxent();

   return (curr_time-boot_time);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   getloadavg( plat_loadavg, 3 );
   *one = plat_loadavg[LOADAVG_1MIN];
   *five = plat_loadavg[LOADAVG_5MIN];
   *fifteen = plat_loadavg[LOADAVG_15MIN];
}

int Platform_getMaxPid() {
   kstat_ctl_t *kc = NULL;
   kstat_t *kshandle = NULL;
   kvar_t *ksvar = NULL;
   int vproc = 32778; // Reasonable Solaris default
   kc = kstat_open();
   if (kc != NULL) { kshandle = kstat_lookup(kc,"unix",0,"var"); }
   if (kshandle != NULL) { kstat_read(kc,kshandle,NULL); }
   ksvar = kshandle->ks_data;
   if (ksvar->v_proc > 0 ) {
      vproc = ksvar->v_proc;
   }
   if (kc != NULL) { kstat_close(kc); }
   return vproc;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   SolarisProcessList* spl = (SolarisProcessList*) this->pl;
   int cpus = this->pl->cpuCount;
   CPUData* cpuData = NULL;

   if (cpus == 1) {
     // single CPU box has everything in spl->cpus[0]
     cpuData = &(spl->cpus[0]);
   } else {
     cpuData = &(spl->cpus[cpu]);
   }

   double percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->systemAllPercent;
      Meter_setItems(this, 3);
      percent = v[0]+v[1]+v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;

   v[CPU_METER_FREQUENCY] = -1;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalMem;
   this->values[0] = pl->usedMem;
   this->values[1] = pl->buffersMem;
   this->values[2] = pl->cachedMem;
}

void Platform_setSwapValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

void Platform_setZfsArcValues(Meter* this) {
   SolarisProcessList* spl = (SolarisProcessList*) this->pl;

   ZfsArcMeter_readStats(this, &(spl->zfs));
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   SolarisProcessList* spl = (SolarisProcessList*) this->pl;

   ZfsCompressedArcMeter_readStats(this, &(spl->zfs));
}

static int Platform_buildenv(void *accum, struct ps_prochandle *Phandle, uintptr_t addr, const char *str) {
   envAccum *accump = accum;
   (void) Phandle;
   (void) addr;
   size_t thissz = strlen(str);
   if ((thissz + 2) > (accump->capacity - accump->size))
      accump->env = xRealloc(accump->env, accump->capacity *= 2);
   if ((thissz + 2) > (accump->capacity - accump->size))
      return 1;
   strlcpy( accump->env + accump->size, str, (accump->capacity - accump->size));
   strncpy( accump->env + accump->size + thissz + 1, "\n", 1);
   accump->size = accump->size + thissz + 1;
   return 0;
}

char* Platform_getProcessEnv(pid_t pid) {
   envAccum envBuilder;
   pid_t realpid = pid / 1024;
   int graberr;
   struct ps_prochandle *Phandle;

   if ((Phandle = Pgrab(realpid,PGRAB_RDONLY,&graberr)) == NULL)
      return "Unable to read process environment.";

   envBuilder.capacity = 4096;
   envBuilder.size     = 0;
   envBuilder.env      = xMalloc(envBuilder.capacity);

   (void) Penv_iter(Phandle,Platform_buildenv,&envBuilder);

   Prelease(Phandle, 0);

   strncpy( envBuilder.env + envBuilder.size, "\0", 1);
   return envBuilder.env;
}
