/*
htop - OpenBSDProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "openbsd/OpenBSDProcessTable.h"

#include <kvm.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uvm/uvmexp.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessTable.h"
#include "Settings.h"
#include "XUtils.h"
#include "openbsd/OpenBSDMachine.h"
#include "openbsd/OpenBSDProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   OpenBSDProcessTable* this = xCalloc(1, sizeof(OpenBSDProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(OpenBSDProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   OpenBSDProcessTable* this = (OpenBSDProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

static void OpenBSDProcessTable_updateCwd(const struct kinfo_proc* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC_CWD, kproc->p_pid };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 3, buffer, &size, NULL, 0) != 0) {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   /* Kernel threads return an empty buffer */
   if (buffer[0] == '\0') {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   free_and_xStrdup(&proc->procCwd, buffer);
}

static void OpenBSDProcessTable_updateProcessName(kvm_t* kd, const struct kinfo_proc* kproc, Process* proc) {
   Process_updateComm(proc, kproc->p_comm);

   /*
    * Like OpenBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   char** arg = kvm_getargv(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   size_t len = 0;
   for (size_t i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }

   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   char* s;
   if ((s = malloc(len)) == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   *s = '\0';

   size_t start = 0;
   size_t end = 0;
   for (size_t i = 0; arg[i] != NULL; i++) {
      size_t n = strlcat(s, arg[i], len);
      if (i == 0) {
         end = MINIMUM(n, len - 1);
         /* check if cmdline ended earlier, e.g 'kdeinit5: Running...' */
         for (size_t j = end; j > 0; j--) {
            if (arg[0][j] == ' ' && arg[0][j - 1] != '\\') {
               end = (arg[0][j - 1] == ':') ? (j - 1) : j;
            }
         }
      }
      /* the trailing space should get truncated anyway */
      strlcat(s, " ", len);
   }

   Process_updateCmdline(proc, s, start, end);

   free(s);
}

/*
 * Taken from OpenBSD's ps(1).
 */
static double getpcpu(const OpenBSDMachine* ohost, const struct kinfo_proc* kp) {
   if (ohost->fscale == 0)
      return 0.0;

   return 100.0 * (double)kp->p_pctcpu / ohost->fscale;
}

static void OpenBSDProcessTable_scanProcs(OpenBSDProcessTable* this) {
   Machine* host = this->super.super.host;
   OpenBSDMachine* ohost = (OpenBSDMachine*) host;
   const Settings* settings = host->settings;
   const bool hideKernelThreads = settings->hideKernelThreads;
   const bool hideUserlandThreads = settings->hideUserlandThreads;
   int count = 0;

   const struct kinfo_proc* kprocs = kvm_getprocs(ohost->kd, KERN_PROC_KTHREAD | KERN_PROC_SHOW_THREADS, 0, sizeof(struct kinfo_proc), &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];

      /* Ignore main threads */
      if (kproc->p_tid != -1) {
         Process* containingProcess = ProcessTable_findProcess(&this->super, kproc->p_pid);
         if (containingProcess) {
            if (((OpenBSDProcess*)containingProcess)->addr == kproc->p_addr)
               continue;

            containingProcess->nlwp++;
         }
      }

      bool preExisting = false;
      Process* proc = ProcessTable_getProcess(&this->super, (kproc->p_tid == -1) ? kproc->p_pid : kproc->p_tid, &preExisting, OpenBSDProcess_new);
      OpenBSDProcess* op = (OpenBSDProcess*) proc;

      if (!preExisting) {
         Process_setParent(proc, kproc->p_ppid);
         Process_setThreadGroup(proc, kproc->p_pid);
         proc->tpgid = kproc->p_tpgid;
         proc->session = kproc->p_sid;
         proc->pgrp = kproc->p__pgid;
         proc->isKernelThread = proc->pgrp == 0;
         proc->isUserlandThread = kproc->p_tid != -1;
         proc->starttime_ctime = kproc->p_ustart_sec;
         Process_fillStarttimeBuffer(proc);
         ProcessTable_add(&this->super, proc);

         OpenBSDProcessTable_updateProcessName(ohost->kd, kproc, proc);

         if (settings->ss->flags & PROCESS_FLAG_CWD) {
            OpenBSDProcessTable_updateCwd(kproc, proc);
         }

         proc->tty_nr = kproc->p_tdev;
         const char* name = ((dev_t)kproc->p_tdev != NODEV) ? devname(kproc->p_tdev, S_IFCHR) : NULL;
         if (!name || String_eq(name, "??")) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }
      } else {
         if (settings->updateProcessNames) {
            OpenBSDProcessTable_updateProcessName(ohost->kd, kproc, proc);
         }
      }

      op->addr = kproc->p_addr;
      proc->m_virt = kproc->p_vm_dsize * ohost->pageSizeKB;
      proc->m_resident = kproc->p_vm_rssize * ohost->pageSizeKB;

      proc->percent_mem = proc->m_resident / (float)ohost->totalMem * 100.0F;
      proc->percent_cpu = CLAMP(getpcpu(ohost, kproc), 0.0F, host->activeCPUs * 100.0F);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->nice = kproc->p_nice - 20;
      proc->time = 100 * (kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000));
      proc->priority = kproc->p_priority - PZERO;
      proc->processor = (int) kproc->p_cpuid;
      proc->minflt = kproc->p_uru_minflt;
      proc->majflt = kproc->p_uru_majflt;
      proc->nlwp = 1;

      if (proc->st_uid != kproc->p_uid) {
         proc->st_uid = kproc->p_uid;
         proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);
      }

      /* Taken from: https://github.com/openbsd/src/blob/6a38af0976a6870911f4b2de19d8da28723a5778/sys/sys/proc.h#L420 */
      switch (kproc->p_stat) {
         case SIDL:    proc->state = IDLE; break;
         case SRUN:    proc->state = RUNNABLE; break;
         case SSLEEP:  proc->state = SLEEPING; break;
         case SSTOP:   proc->state = STOPPED; break;
         case SZOMB:   proc->state = ZOMBIE; break;
         case SDEAD:   proc->state = DEFUNCT; break;
         case SONPROC: proc->state = RUNNING; break;
         default:      proc->state = UNKNOWN;
      }

      if (Process_isKernelThread(proc)) {
         this->super.kernelThreads++;
      } else if (Process_isUserlandThread(proc)) {
         this->super.userlandThreads++;
      }

      this->super.totalTasks++;
      if (proc->state == RUNNING) {
         this->super.runningTasks++;
      }

      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));
      proc->super.updated = true;
   }
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   OpenBSDProcessTable* this = (OpenBSDProcessTable*) super;

   OpenBSDProcessTable_scanProcs(this);
}
