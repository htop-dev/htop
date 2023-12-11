/*
htop - ProcessTable.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProcessTable.h"

#include <assert.h>
#include <stdlib.h>

#include "Hashtable.h"
#include "Row.h"
#include "Settings.h"
#include "Vector.h"


void ProcessTable_init(ProcessTable* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList) {
   Table_init(&this->super, klass, host);

   this->pidMatchList = pidMatchList;
}

void ProcessTable_done(ProcessTable* this) {
   Table_done(&this->super);
}

Process* ProcessTable_getProcess(ProcessTable* this, pid_t pid, bool* preExisting, Process_New constructor) {
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

static void ProcessTable_prepareEntries(Table* super) {
   ProcessTable* this = (ProcessTable*) super;
   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;

   Table_prepareEntries(super);
}

static void ProcessTable_iterateEntries(Table* super) {
   ProcessTable* this = (ProcessTable*) super;
   // calling into platform-specific code
   ProcessTable_goThroughEntries(this);
}

static void ProcessTable_cleanupEntries(Table* super) {
   Machine* host = super->host;
   const Settings* settings = host->settings;

   // Finish process table update, culling any exit'd processes
   for (int i = Vector_size(super->rows) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(super->rows, i);

      // tidy up Process state after refreshing the ProcessTable table
      Process_makeCommandStr(p, settings);

      // keep track of the highest UID for column scaling
      if (p->st_uid > host->maxUserId)
         host->maxUserId = p->st_uid;

      Table_cleanupRow(super, (Row*) p, i);
   }

   // compact the table in case of deletions
   Table_compact(super);
}

const TableClass ProcessTable_class = {
   .super = {
      .extends = Class(Table),
      .delete = ProcessTable_delete,
   },
   .prepare = ProcessTable_prepareEntries,
   .iterate = ProcessTable_iterateEntries,
   .cleanup = ProcessTable_cleanupEntries,
};
