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
#include "Machine.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "RichString.h"
#include "Settings.h"
#include "UsersTable.h"
#include "Vector.h"


typedef struct ProcessList_ {
   struct Machine_* host;

   Vector* processes;         /* all known processes; sort order can vary and differ from display order */
   Vector* displayList;       /* process tree flattened in display order (borrowed);
                                 updated in ProcessList_updateDisplayList when rebuilding panel */
   Hashtable* processTable;   /* fast known process lookup by PID */

   bool needsSort;

   Panel* panel;
   int following;
   const char* incFilter;
   Hashtable* pidMatchList;

   unsigned int totalTasks;
   unsigned int runningTasks;
   unsigned int userlandThreads;
   unsigned int kernelThreads;
} ProcessList;

/* Implemented by platforms */
ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList);
void ProcessList_delete(ProcessList* this);
void ProcessList_goThroughEntries(ProcessList* this);

void ProcessList_init(ProcessList* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList);

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

void ProcessList_scan(ProcessList* this);

static inline Process* ProcessList_findProcess(ProcessList* this, pid_t pid) {
   return (Process*) Hashtable_get(this->processTable, pid);
}

#endif
