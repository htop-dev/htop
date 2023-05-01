/*
htop - UnsupportedProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "UnsupportedProcessList.h"

#include <stdlib.h>
#include <string.h>

#include "ProcessList.h"
#include "UnsupportedProcess.h"


ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList) {
   ProcessList* this = xCalloc(1, sizeof(ProcessList));
   ProcessList_init(this, Class(Process), host, pidMatchList);

   host->existingCPUs = 1;
   host->activeCPUs = 1;

   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   bool preExisting = true;
   Process* proc;

   proc = ProcessList_getProcess(super, 1, &preExisting, UnsupportedProcess_new);

   /* Empty values */
   proc->time = proc->time + 10;
   proc->pid  = 1;
   proc->ppid = 1;
   proc->tgid = 0;

   Process_updateComm(proc, "commof16char");
   Process_updateCmdline(proc, "<unsupported architecture>", 0, 0);
   Process_updateExe(proc, "/path/to/executable");

   if (proc->settings->ss->flags & PROCESS_FLAG_CWD) {
      free_and_xStrdup(&proc->procCwd, "/current/working/directory");
   }

   proc->updated = true;

   proc->state = RUNNING;
   proc->isKernelThread = false;
   proc->isUserlandThread = false;
   proc->show = true; /* Reflected in proc->settings-> "hideXXX" really */
   proc->pgrp = 0;
   proc->session = 0;
   proc->tty_nr = 0;
   proc->tty_name = NULL;
   proc->tpgid = 0;
   proc->processor = 0;

   proc->percent_cpu = 2.5;
   proc->percent_mem = 2.5;
   Process_updateCPUFieldWidths(proc->percent_cpu);

   proc->st_uid = 0;
   proc->user = "nobody"; /* Update whenever proc->st_uid is changed */

   proc->priority = 0;
   proc->nice = 0;
   proc->nlwp = 1;
   proc->starttime_ctime = 1433116800; // Jun 01, 2015
   Process_fillStarttimeBuffer(proc);

   proc->m_virt = 100;
   proc->m_resident = 100;

   proc->minflt = 20;
   proc->majflt = 20;

   if (!preExisting)
      ProcessList_add(super, proc);
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   Machine* this = xCalloc(1, sizeof(Machine));
   Machine_init(this, usersTable, userId);
   return this;
}

void Machine_delete(Machine* host) {
   free(host);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);

   (void) host; (void) id;

   return true;
}
