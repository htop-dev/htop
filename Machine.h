#ifndef HEADER_Machine
#define HEADER_Machine
/*
htop - Machine.h
(C) 2023 Red Hat, Inc.
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Panel.h"
#include "Settings.h"
#include "Table.h"
#include "UsersTable.h"

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#endif


#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

typedef unsigned long long int memory_t;
#define MEMORY_MAX ULLONG_MAX

typedef struct Machine_ {
   struct Settings_* settings;

   struct timeval realtime;   /* time of the current sample */
   uint64_t realtimeMs;       /* current time in milliseconds */
   uint64_t monotonicMs;      /* same, but from monotonic clock */
   uint64_t prevMonotonicMs;  /* time in milliseconds from monotonic clock of previous scan */

   int64_t iterationsRemaining;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   memory_t totalMem;

   memory_t totalSwap;
   memory_t usedSwap;
   memory_t cachedSwap;

   unsigned int activeCPUs;
   unsigned int existingCPUs;

   UsersTable* usersTable;
   uid_t htopUserId;
   uid_t maxUserId;  /* recently observed */
   uid_t userId;  /* selected row user ID */

   pid_t maxProcessId; /* largest PID seen at runtime */

   size_t tableCount;
   Table **tables;
   Table *activeTable;
   Table *processTable;
} Machine;


Machine* Machine_new(UsersTable* usersTable, uid_t userId);

void Machine_init(Machine* this, UsersTable* usersTable, uid_t userId);

void Machine_delete(Machine* this);

void Machine_done(Machine* this);

bool Machine_isCPUonline(const Machine* this, unsigned int id);

void Machine_populateTablesFromSettings(Machine* this, Settings* settings, Table* processTable);

void Machine_setTablesPanel(Machine* this, Panel* panel);

void Machine_scan(Machine* this);

void Machine_scanTables(Machine* this);

#endif
