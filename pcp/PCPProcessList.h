#ifndef HEADER_PCPProcessList
#define HEADER_PCPProcessList
/*
htop - PCPProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

#include "pcp/Platform.h"
#include "zfs/ZfsArcStats.h"


typedef enum CPUMetric_ {
   CPU_TOTAL_TIME,
   CPU_USER_TIME,
   CPU_SYSTEM_TIME,
   CPU_SYSTEM_ALL_TIME,
   CPU_IDLE_ALL_TIME,
   CPU_IDLE_TIME,
   CPU_NICE_TIME,
   CPU_IOWAIT_TIME,
   CPU_IRQ_TIME,
   CPU_SOFTIRQ_TIME,
   CPU_STEAL_TIME,
   CPU_GUEST_TIME,
   CPU_GUESTNICE_TIME,

   CPU_TOTAL_PERIOD,
   CPU_USER_PERIOD,
   CPU_SYSTEM_PERIOD,
   CPU_SYSTEM_ALL_PERIOD,
   CPU_IDLE_ALL_PERIOD,
   CPU_IDLE_PERIOD,
   CPU_NICE_PERIOD,
   CPU_IOWAIT_PERIOD,
   CPU_IRQ_PERIOD,
   CPU_SOFTIRQ_PERIOD,
   CPU_STEAL_PERIOD,
   CPU_GUEST_PERIOD,
   CPU_GUESTNICE_PERIOD,

   CPU_FREQUENCY,

   CPU_METRIC_COUNT
} CPUMetric;

typedef struct PCPProcessList_ {
   ProcessList super;
   double timestamp;		/* previous sample timestamp */
   pmAtomValue* cpu;		/* aggregate values for each metric */
   pmAtomValue** percpu;	/* per-processor values for each metric */
   pmAtomValue* values;		/* per-processor buffer for just one metric */
   ZfsArcStats zfs;
} PCPProcessList;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* pl);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

#endif
