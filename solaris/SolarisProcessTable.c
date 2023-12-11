/*
htop - SolarisProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "solaris/SolarisProcessTable.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/user.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <time.h>

#include "CRT.h"
#include "solaris/Platform.h"
#include "solaris/SolarisProcess.h"


#define GZONE "global    "
#define UZONE "unknown   "

static char* SolarisProcessTable_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
   char* zname;

   if ( sproc->zoneid == 0 ) {
      zname = xStrdup(GZONE);
   } else if ( kd == NULL ) {
      zname = xStrdup(UZONE);
   } else {
      kstat_t* ks = kstat_lookup_wrapper( kd, "zones", sproc->zoneid, NULL );
      zname = xStrdup(ks == NULL ? UZONE : ks->ks_name);
   }

   return zname;
}

ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   SolarisProcessTable* this = xCalloc(1, sizeof(SolarisProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(SolarisProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   SolarisProcessTable* this = (SolarisProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

static void SolarisProcessTable_updateExe(pid_t pid, Process* proc) {
   char path[32];
   xSnprintf(path, sizeof(path), "/proc/%d/path/a.out", pid);

   char target[PATH_MAX];
   ssize_t ret = readlink(path, target, sizeof(target) - 1);
   if (ret <= 0)
      return;

   target[ret] = '\0';
   Process_updateExe(proc, target);
}

static void SolarisProcessTable_updateCwd(pid_t pid, Process* proc) {
   char path[32];
   xSnprintf(path, sizeof(path), "/proc/%d/cwd", pid);

   char target[PATH_MAX];
   ssize_t ret = readlink(path, target, sizeof(target) - 1);
   if (ret <= 0)
      return;

   target[ret] = '\0';
   free_and_xStrdup(&proc->procCwd, target);
}

/* Taken from: https://docs.oracle.com/cd/E19253-01/817-6223/6mlkidlom/index.html#tbl-sched-state */
static inline ProcessState SolarisProcessTable_getProcessState(char state) {
   switch (state) {
      case 'S': return SLEEPING;
      case 'R': return RUNNABLE;
      case 'O': return RUNNING;
      case 'Z': return ZOMBIE;
      case 'T': return STOPPED;
      case 'I': return IDLE;
      default: return UNKNOWN;
   }
}

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */

static int SolarisProcessTable_walkproc(psinfo_t* _psinfo, lwpsinfo_t* _lwpsinfo, void* listptr) {
   bool preExisting;
   pid_t getpid;

   // Setup process list
   ProcessTable* pt = (ProcessTable*) listptr;
   SolarisProcessTable* spt = (SolarisProcessTable*) listptr;
   Machine* host = pt->host;

   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) {
      return 0;
   }

   pid_t lwpid   = (_psinfo->pr_pid * 1024) + lwpid_real;
   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);
   if (onMasterLWP) {
      getpid = _psinfo->pr_pid * 1024;
   } else {
      getpid = lwpid;
   }

   Process* proc            = ProcessTable_getProcess(pt, getpid, &preExisting, SolarisProcess_new);
   SolarisProcess* sproc    = (SolarisProcess*) proc;
   const Settings* settings = host->settings;

   // Common code pass 1
   proc->show               = false;
   sproc->taskid            = _psinfo->pr_taskid;
   sproc->projid            = _psinfo->pr_projid;
   sproc->poolid            = _psinfo->pr_poolid;
   sproc->contid            = _psinfo->pr_contract;
   proc->priority           = _lwpsinfo->pr_pri;
   proc->nice               = _lwpsinfo->pr_nice - NZERO;
   proc->processor          = _lwpsinfo->pr_onpro;
   proc->state              = SolarisProcessTable_getProcessState(_lwpsinfo->pr_sname);
   // NOTE: This 'percentage' is a 16-bit BINARY FRACTIONS where 1.0 = 0x8000
   // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
   // (accessed on 18 November 2017)
   proc->percent_mem        = ((uint16_t)_psinfo->pr_pctmem / (double)32768) * (double)100.0;
   proc->pgrp               = _psinfo->pr_pgid;
   proc->nlwp               = _psinfo->pr_nlwp;
   proc->session            = _psinfo->pr_sid;

   proc->tty_nr             = _psinfo->pr_ttydev;
   const char* name = (_psinfo->pr_ttydev != PRNODEV) ? ttyname(_psinfo->pr_ttydev) : NULL;
   if (!name) {
      free(proc->tty_name);
      proc->tty_name = NULL;
   } else {
      free_and_xStrdup(&proc->tty_name, name);
   }

   proc->m_resident         = _psinfo->pr_rssize;  // KB
   proc->m_virt             = _psinfo->pr_size;    // KB

   if (proc->st_uid != _psinfo->pr_euid) {
      proc->st_uid          = _psinfo->pr_euid;
      proc->user            = UsersTable_getRef(host->usersTable, proc->st_uid);
   }

   if (!preExisting) {
      sproc->realpid        = _psinfo->pr_pid;
      sproc->lwpid          = lwpid_real;
      sproc->zoneid         = _psinfo->pr_zoneid;
      sproc->zname          = SolarisProcessTable_readZoneName(spt->kd, sproc);
      SolarisProcessTable_updateExe(_psinfo->pr_pid, proc);

      Process_updateComm(proc, _psinfo->pr_fname);
      Process_updateCmdline(proc, _psinfo->pr_psargs, 0, 0);

      if (settings->ss->flags & PROCESS_FLAG_CWD) {
         SolarisProcessTable_updateCwd(_psinfo->pr_pid, proc);
      }
   }

   // End common code pass 1

   if (onMasterLWP) { // Are we on the representative LWP?
      Process_setParent(proc, (_psinfo->pr_ppid * 1024));
      Process_setThreadGroup(proc, (_psinfo->pr_ppid * 1024));
      sproc->realppid       = _psinfo->pr_ppid;
      sproc->realtgid       = _psinfo->pr_ppid;

      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu / (double)32768) * (double)100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->time            = _psinfo->pr_time.tv_sec * 100 + _psinfo->pr_time.tv_nsec / 10000000;
      if (!preExisting) { // Tasks done only for NEW processes
         proc->isUserlandThread = false;
         proc->starttime_ctime = _psinfo->pr_start.tv_sec;
      }

      // Update proc and thread counts based on settings
      if (proc->isKernelThread && !settings->hideKernelThreads) {
         pt->kernelThreads += proc->nlwp;
         pt->totalTasks += proc->nlwp + 1;
         if (proc->state == RUNNING) {
            pt->runningTasks++;
         }
      } else if (!proc->isKernelThread) {
         if (proc->state == RUNNING) {
            pt->runningTasks++;
         }
         if (settings->hideUserlandThreads) {
            pt->totalTasks++;
         } else {
            pt->userlandThreads += proc->nlwp;
            pt->totalTasks += proc->nlwp + 1;
         }
      }
      proc->show = !(settings->hideKernelThreads && proc->isKernelThread);
   } else { // We are not in the master LWP, so jump to the LWP handling code
      proc->percent_cpu        = ((uint16_t)_lwpsinfo->pr_pctcpu / (double)32768) * (double)100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->time               = _lwpsinfo->pr_time.tv_sec * 100 + _lwpsinfo->pr_time.tv_nsec / 10000000;
      if (!preExisting) { // Tasks done only for NEW LWPs
         proc->isUserlandThread    = true;
         Process_setParent(proc, _psinfo->pr_pid * 1024);
         Process_setThreadGroup(proc, _psinfo->pr_pid * 1024);
         sproc->realppid       = _psinfo->pr_pid;
         sproc->realtgid       = _psinfo->pr_pid;
         proc->starttime_ctime = _lwpsinfo->pr_start.tv_sec;
      }

      // Top-level process only gets this for the representative LWP
      if (proc->isKernelThread && !settings->hideKernelThreads) {
         proc->super.show = true;
      }
      if (!proc->isKernelThread && !settings->hideUserlandThreads) {
         proc->super.show = true;
      }
   } // Top-level LWP or subordinate LWP

   // Common code pass 2

   if (!preExisting) {
      if ((sproc->realppid <= 0) && !(sproc->realpid <= 1)) {
         proc->isKernelThread = true;
      } else {
         proc->isKernelThread = false;
      }

      Process_fillStarttimeBuffer(proc);
      ProcessTable_add(pt, proc);
   }

   proc->super.updated = true;

   // End common code pass 2

   return 0;
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   super->kernelThreads = 1;
   proc_walk(&SolarisProcessTable_walkproc, super, PR_WALK_LWP);
}
