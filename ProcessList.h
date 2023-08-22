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
#include "Table.h"


typedef struct ProcessList_ {
   Table super;

   Hashtable* pidMatchList;

   unsigned int totalTasks;
   unsigned int runningTasks;
   unsigned int userlandThreads;
   unsigned int kernelThreads;
} ProcessList;

/* Implemented by platforms */
ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList);
void ProcessList_delete(Object* cast);
void ProcessList_goThroughEntries(ProcessList* this);

void ProcessList_init(ProcessList* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList);

void ProcessList_done(ProcessList* this);

extern const TableClass ProcessList_class;

static inline void ProcessList_add(ProcessList* this, Process* process) {
   Table_add(&this->super, &process->super);
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor);

static inline Process* ProcessList_findProcess(ProcessList* this, pid_t pid) {
   return (Process*) Table_findRow(&this->super, pid);
}

#endif
