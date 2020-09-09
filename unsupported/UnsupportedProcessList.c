/*
htop - UnsupportedProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "UnsupportedProcess.h"

#include <stdlib.h>
#include <string.h>


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   ProcessList* this = xCalloc(1, sizeof(ProcessList));
   ProcessList_init(this, Class(Process), usersTable, pidMatchList, userId);

   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super) {
	bool preExisting = true;
    Process *proc;

    proc = ProcessList_getProcess(super, 1, &preExisting, UnsupportedProcess_new);

    /* Empty values */
    proc->time = proc->time + 10;
    proc->pid  = 1;
    proc->ppid = 1;
    proc->tgid = 0;
    proc->comm = "<unsupported architecture>";
    proc->basenameOffset = 0;
    proc->updated = true;

    proc->state = 'R';
    proc->show = true; /* Reflected in proc->settings-> "hideXXX" really */
    proc->pgrp = 0;
    proc->session = 0;
    proc->tty_nr = 0;
    proc->tpgid = 0;
    proc->st_uid = 0;
    proc->flags = 0;
    proc->processor = 0;

    proc->percent_cpu = 2.5;
    proc->percent_mem = 2.5;
    proc->user = "nobody";

    proc->priority = 0;
    proc->nice = 0;
    proc->nlwp = 1;
    strncpy(proc->starttime_show, "Jun 01 ", sizeof(proc->starttime_show));
    proc->starttime_ctime = 1433116800; // Jun 01, 2015

    proc->m_size = 100;
    proc->m_resident = 100;

    proc->minflt = 20;
    proc->majflt = 20;
}
