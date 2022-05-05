#ifndef HEADER_ProcessList
#define HEADER_ProcessList
/*
htop - ProcessList.h
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
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "RichString.h"
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

typedef struct ProcessList_ {
   const Settings* settings;

   Vector* processes;         /* all known processes; sort order can vary and differ from display order */
   Vector* displayList;       /* process tree flattened in display order (borrowed);
                                 updated in ProcessList_updateDisplayList when rebuilding panel */
   Hashtable* processTable;   /* fast known process lookup by PID */
   UsersTable* usersTable;

   bool needsSort;

   Hashtable* dynamicMeters;  /* runtime-discovered meters */
   Hashtable* dynamicColumns; /* runtime-discovered Columns */

   struct timeval realtime;   /* time of the current sample */
   uint64_t realtimeMs;       /* current time in milliseconds */
   uint64_t monotonicMs;      /* same, but from monotonic clock */

   Panel* panel;
   int following;
   uid_t userId;
   const char* incFilter;
   Hashtable* pidMatchList;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   unsigned int totalTasks;
   unsigned int runningTasks;
   unsigned int userlandThreads;
   unsigned int kernelThreads;

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
} ProcessList;

/* Implemented by platforms */
ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);
void ProcessList_delete(ProcessList* pl);
void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);
bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);


ProcessList* ProcessList_init(ProcessList* this, const ObjectClass* klass, UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ProcessList_done(ProcessList* this);

void ProcessList_setPanel(ProcessList* this, Panel* panel);

void ProcessList_printHeader(const ProcessList* this, RichString* header);

void ProcessList_add(ProcessList* this, Process* p);

void ProcessList_updateDisplayList(ProcessList* this);

ProcessField ProcessList_keyAt(const ProcessList* this, int at);

void ProcessList_expandTree(ProcessList* this);

void ProcessList_collapseAllBranches(ProcessList* this);

void ProcessList_rebuildPanel(ProcessList* this);

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor);

void ProcessList_scan(ProcessList* this, bool pauseProcessUpdate);

static inline Process* ProcessList_findProcess(ProcessList* this, pid_t pid) {
   return (Process*) Hashtable_get(this->processTable, pid);
}

#endif
