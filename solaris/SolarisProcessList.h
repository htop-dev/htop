#ifndef HEADER_SolarisProcessList
#define HEADER_SolarisProcessList
/*
htop - SolarisProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define MAXCMDLINE 255

#define GZONE	"global    "
#define UZONE	"unknown   "

#include "zfs/ZfsArcStats.h"

#include <kstat.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>

#define ZONE_ERRMSGLEN 1024
extern char zone_errmsg[ZONE_ERRMSGLEN];

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   CPUData* cpus;
   ZfsArcStats zfs;
} SolarisProcessList;

char* SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc);

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* pl);

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */

int SolarisProcessList_walkproc(psinfo_t *_psinfo, lwpsinfo_t *_lwpsinfo, void *listptr);

void ProcessList_goThroughEntries(ProcessList* this);

#endif
