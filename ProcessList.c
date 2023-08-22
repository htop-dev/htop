/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Platform.h"
#include "Vector.h"
#include "XUtils.h"


void ProcessList_init(ProcessList* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList) {
   Table_init(&this->super, klass, host);

   this->pidMatchList = pidMatchList;
}

void ProcessList_done(ProcessList* this) {
   Table_done(&this->super);
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor) {
   const Table* table = &this->super;
   Process* proc = (Process*) Hashtable_get(table->table, pid);
   *preExisting = proc != NULL;
   if (proc) {
      assert(Vector_indexOf(table->rows, proc, Row_idEqualCompare) != -1);
      assert(Process_getPid(proc) == pid);
   } else {
      proc = constructor(table->host);
      assert(proc->cmdline == NULL);
      Process_setPid(proc, pid);
   }
   return proc;
}

static void ProcessList_prepareEntries(Table* super) {
   ProcessList* this = (ProcessList*) super;
   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;

   Table_prepareEntries(super);
}

static void ProcessList_iterateEntries(Table* super) {
   ProcessList* this = (ProcessList*) super;
   // calling into platform-specific code
   ProcessList_goThroughEntries(this);
}

static void ProcessList_cleanupEntries(Table* super) {
   Machine* host = super->host;
   const Settings* settings = host->settings;

   // Finish process table update, culling any exit'd processes
   for (int i = Vector_size(super->rows) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(super->rows, i);

      // tidy up Process state after refreshing the ProcessList table
      Process_makeCommandStr(p, settings);

      // keep track of the highest UID for column scaling
      if (p->st_uid > host->maxUserId)
         host->maxUserId = p->st_uid;

      Table_cleanupRow(super, (Row*) p, i);
   }

   // compact the table in case of deletions
   Table_compact(super);
}

const TableClass ProcessList_class = {
   .super = {
      .extends = Class(Table),
      .delete = ProcessList_delete,
   },
   .prepare = ProcessList_prepareEntries,
   .iterate = ProcessList_iterateEntries,
   .cleanup = ProcessList_cleanupEntries,
};
