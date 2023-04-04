#ifndef HEADER_SolarisProcessList
#define HEADER_SolarisProcessList
/*
htop - SolarisProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <kstat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

#include "solaris/SolarisProcess.h"

#include "zfs/ZfsArcStats.h"


#define ZONE_ERRMSGLEN 1024
extern char zone_errmsg[ZONE_ERRMSGLEN];

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   double frequency;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
   bool online;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   CPUData* cpus;
   ZfsArcStats zfs;
} SolarisProcessList;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* pl);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);

#endif
