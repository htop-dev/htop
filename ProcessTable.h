#ifndef HEADER_ProcessTable
#define HEADER_ProcessTable
/*
htop - ProcessTable.h
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Machine.h"
#include "Object.h"
#include "Process.h"
#include "Table.h"


typedef struct ProcessTable_ {
   Table super;

   Hashtable* pidMatchList;

   unsigned int totalTasks;
   unsigned int runningTasks;
   unsigned int userlandThreads;
   unsigned int kernelThreads;
} ProcessTable;

/* Implemented by platforms */
ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList);
void ProcessTable_delete(Object* cast);
void ProcessTable_goThroughEntries(ProcessTable* this);

void ProcessTable_init(ProcessTable* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList);

void ProcessTable_done(ProcessTable* this);

extern const TableClass ProcessTable_class;

static inline void ProcessTable_add(ProcessTable* this, Process* process) {
   Table_add(&this->super, &process->super);
}

Process* ProcessTable_getProcess(ProcessTable* this, pid_t pid, bool* preExisting, Process_New constructor);

static inline Process* ProcessTable_findProcess(ProcessTable* this, pid_t pid) {
   return (Process*) Table_findRow(&this->super, pid);
}

#endif
