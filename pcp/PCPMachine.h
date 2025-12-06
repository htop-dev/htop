#ifndef HEADER_PCPMachine
#define HEADER_PCPMachine
/*
htop - PCPMachine.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Machine.h"
#include "UsersTable.h"

#include "pcp/Platform.h"
#include "linux/ZswapStats.h"
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

typedef enum MemoryMetric_ {
   // Linux
   MEMORY_CLASS_USED = 0,
   MEMORY_CLASS_SHARED = 1,
   MEMORY_CLASS_BUFFERS = 2,
   MEMORY_CLASS_CACHE = 3,
   MEMORY_CLASS_COMPRESSED = 4,
   MEMORY_CLASS_AVAILABLE = 5,
   // Darwin
   MEMORY_CLASS_WIRED = 0,
   MEMORY_CLASS_SPECULATIVE = 1,
   MEMORY_CLASS_ACTIVE = 2,
   MEMORY_CLASS_PURGEABLE = 3,
   MEMORY_CLASS_INACTIVE = 5,
   // Maximum
   MEMORY_CLASS_LIMIT = 6
} MemoryMetric;

typedef enum SystemName_ {
   SYSTEM_NAME_LINUX,
   SYSTEM_NAME_DARWIN,
   SYSTEM_NAME_UNKNOWN
} SystemName;

typedef struct PCPMachine_ {
   Machine super;
   SystemName sys;
   int smaps_flag;
   double period;
   double timestamp;     /* previous sample timestamp */

   memory_t memValue[MEMORY_CLASS_LIMIT];

   pmAtomValue* cpu;     /* aggregate values for each metric */
   pmAtomValue** percpu; /* per-processor values for each metric */
   pmAtomValue* values;  /* per-processor buffer for just one metric */

   ZfsArcStats zfs;
   /*ZramStats zram; -- not needed, calculated in-line in Platform.c */
   ZswapStats zswap;
} PCPMachine;

#endif
