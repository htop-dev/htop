#ifndef HEADER_SolarisMachine
#define HEADER_SolarisMachine
/*
htop - SolarisMachine.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kstat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/swap.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/uio.h>

#include "Hashtable.h"
#include "UsersTable.h"

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

typedef struct SolarisMachine_ {
   Machine super;

   kstat_ctl_t* kd;
   CPUData* cpus;

   int pageSize;
   int pageSizeKB;

   ZfsArcStats zfs;
} SolarisMachine;

#endif
