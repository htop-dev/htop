/*
htop - solaris/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "solaris/Platform.h"

#include <kstat.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <utmpx.h>
#include <sys/loadavg.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/var.h>

#include "Macros.h"
#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "HostnameMeter.h"
#include "SysArchMeter.h"
#include "UptimeMeter.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsCompressedArcMeter.h"
#include "SolarisProcess.h"
#include "SolarisProcessList.h"


const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Default",
      .columns = "PID LWPID USER PRIORITY NICE M_VIRT M_RESIDENT STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

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
   int boot_time = 0;
   int curr_time = time(NULL);
   struct utmpx* ent;

   while (( ent = getutxent() )) {
      if ( String_eq("system boot", ent->ut_line )) {
         boot_time = ent->ut_tv.tv_sec;
      }
   }

   endutxent();

   return (curr_time - boot_time);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   double plat_loadavg[3];
   if (getloadavg( plat_loadavg, 3 ) < 0) {
      *one = NAN;
      *five = NAN;
      *fifteen = NAN;
      return;
   }
   *one = plat_loadavg[LOADAVG_1MIN];
   *five = plat_loadavg[LOADAVG_5MIN];
   *fifteen = plat_loadavg[LOADAVG_15MIN];
}

int Platform_getMaxPid() {
   int vproc = 32778; // Reasonable Solaris default

   kstat_ctl_t* kc = kstat_open();
   if (kc != NULL) {
      kstat_t* kshandle = kstat_lookup_wrapper(kc, "unix", 0, "var");
      if (kshandle != NULL) {
         kstat_read(kc, kshandle, NULL);

         kvar_t* ksvar = kshandle->ks_data;
         if (ksvar && ksvar->v_proc > 0) {
            vproc = ksvar->v_proc;
         }
      }
      kstat_close(kc);
   }

   return vproc;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const SolarisProcessList* spl = (const SolarisProcessList*) this->pl;
   unsigned int cpus = this->pl->existingCPUs;
   const CPUData* cpuData = NULL;

   if (cpus == 1) {
      // single CPU box has everything in spl->cpus[0]
      cpuData = &(spl->cpus[0]);
   } else {
      cpuData = &(spl->cpus[cpu]);
   }

   if (!cpuData->online) {
      this->curItems = 0;
      return NAN;
   }

   double percent;
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

   percent = isnan(percent) ? 0.0 : CLAMP(percent, 0.0, 100.0);

   v[CPU_METER_FREQUENCY] = cpuData->frequency;
   v[CPU_METER_TEMPERATURE] = NAN;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalMem;
   this->values[0] = pl->usedMem;
   this->values[1] = pl->buffersMem;
   // this->values[2] = "shared memory, like tmpfs and shm"
   this->values[3] = pl->cachedMem;
   // this->values[4] = "available memory"
}

void Platform_setSwapValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
   this->values[1] = NAN;
}

void Platform_setZfsArcValues(Meter* this) {
   const SolarisProcessList* spl = (const SolarisProcessList*) this->pl;

   ZfsArcMeter_readStats(this, &(spl->zfs));
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   const SolarisProcessList* spl = (const SolarisProcessList*) this->pl;

   ZfsCompressedArcMeter_readStats(this, &(spl->zfs));
}

static int Platform_buildenv(void* accum, struct ps_prochandle* Phandle, uintptr_t addr, const char* str) {
   envAccum* accump = accum;
   (void) Phandle;
   (void) addr;

   size_t thissz = strlen(str);

   while ((thissz + 2) > (accump->capacity - accump->size)) {
      if (accump->capacity > (SIZE_MAX / 2))
         return 1;

      accump->capacity *= 2;
      accump->env = xRealloc(accump->env, accump->capacity);
   }

   strlcpy( accump->env + accump->size, str, accump->capacity - accump->size);
   strncpy( accump->env + accump->size + thissz + 1, "\n", 2);

   accump->size += thissz + 1;
   return 0;
}

char* Platform_getProcessEnv(pid_t pid) {
   envAccum envBuilder;
   pid_t realpid = pid / 1024;
   int graberr;
   struct ps_prochandle* Phandle;

   if ((Phandle = Pgrab(realpid, PGRAB_RDONLY, &graberr)) == NULL) {
      return NULL;
   }

   envBuilder.capacity = 4096;
   envBuilder.size     = 0;
   envBuilder.env      = xMalloc(envBuilder.capacity);

   (void) Penv_iter(Phandle, Platform_buildenv, &envBuilder);

   Prelease(Phandle, 0);

   strncpy( envBuilder.env + envBuilder.size, "\0", 1);

   return xRealloc(envBuilder.env, envBuilder.size + 1);
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
   *percent = NAN;
   *isOnAC = AC_ERROR;
}
