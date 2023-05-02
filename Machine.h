#ifndef HEADER_Machine
#define HEADER_Machine
/*
htop - Machine.h
(C) 2023 Red Hat, Inc.
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Settings.h"
#include "UsersTable.h"
#include "Vector.h"

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

struct ProcessList_;

typedef struct Machine_ {
   Settings* settings;

   struct timeval realtime;   /* time of the current sample */
   uint64_t realtimeMs;       /* current time in milliseconds */
   uint64_t monotonicMs;      /* same, but from monotonic clock */

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   memory_t totalMem;
   memory_t usedMem;
   memory_t buffersMem;
   memory_t cachedMem;
   memory_t sharedMem;
   memory_t availableMem;

   memory_t totalSwap;
   memory_t usedSwap;
   memory_t cachedSwap;

   unsigned int activeCPUs;
   unsigned int existingCPUs;

   UsersTable* usersTable;
   uid_t userId;

   /* To become an array of lists - processes, cgroups, filesystems,... etc */
   /* for now though, just point back to the one list we have at the moment */
   struct ProcessList_ *pl;
} Machine;


Machine* Machine_new(UsersTable* usersTable, uid_t userId);

void Machine_init(Machine* this, UsersTable* usersTable, uid_t userId);

void Machine_delete(Machine* this);

void Machine_done(Machine* this);

bool Machine_isCPUonline(const Machine* this, unsigned int id);

void Machine_addList(Machine* this, struct ProcessList_ *pl);

void Machine_scan(Machine* this);

#endif
