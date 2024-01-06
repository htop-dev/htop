/*
htop - NetBSDProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "netbsd/NetBSDProcessTable.h"

#include <kvm.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uvm/uvm_extern.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessTable.h"
#include "Settings.h"
#include "XUtils.h"
#include "netbsd/NetBSDMachine.h"
#include "netbsd/NetBSDProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   NetBSDProcessTable* this = xCalloc(1, sizeof(NetBSDProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = (ProcessTable*) this;
   ProcessTable_init(super, Class(NetBSDProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   NetBSDProcessTable* this = (NetBSDProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

static void NetBSDProcessTable_updateExe(const struct kinfo_proc2* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC_ARGS, kproc->p_pid, KERN_PROC_PATHNAME };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
      Process_updateExe(proc, NULL);
      return;
   }

   /* Kernel threads return an empty buffer */
   if (buffer[0] == '\0') {
      Process_updateExe(proc, NULL);
      return;
   }

   Process_updateExe(proc, buffer);
}

static void NetBSDProcessTable_updateCwd(const struct kinfo_proc2* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC_ARGS, kproc->p_pid, KERN_PROC_CWD };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
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

static void NetBSDProcessTable_updateProcessName(kvm_t* kd, const struct kinfo_proc2* kproc, Process* proc) {
   Process_updateComm(proc, kproc->p_comm);

   /*
    * Like NetBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   char** arg = kvm_getargv2(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   size_t len = 0;
   for (int i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }

   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   char* s;
   if ((s = malloc(len)) == NULL) {
      Process_updateCmdline(proc, kproc->p_comm, 0, strlen(kproc->p_comm));
      return;
   }

   *s = '\0';

   int start = 0;
   int end = 0;
   for (int i = 0; arg[i] != NULL; i++) {
      size_t n = strlcat(s, arg[i], len);
      if (i == 0) {
         end = MINIMUM(n, len - 1);
         /* check if cmdline ended earlier, e.g 'kdeinit5: Running...' */
         for (int j = end; j > 0; j--) {
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
 * Borrowed with modifications from NetBSD's top(1).
 */
static double getpcpu(const NetBSDMachine* nhost, const struct kinfo_proc2* kp) {
   if (nhost->fscale == 0)
      return 0.0;

   return 100.0 * (double)kp->p_pctcpu / nhost->fscale;
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   const Machine* host = super->super.host;
   const NetBSDMachine* nhost = (const NetBSDMachine*) host;
   const Settings* settings = host->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   int count = 0;

   const struct kinfo_proc2* kprocs = kvm_getproc2(nhost->kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2), &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc2* kproc = &kprocs[i];

      bool preExisting = false;
      Process* proc = ProcessTable_getProcess(super, kproc->p_pid, &preExisting, NetBSDProcess_new);

      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         Process_setPid(proc, kproc->p_pid);
         Process_setParent(proc, kproc->p_ppid);
         Process_setThreadGroup(proc, kproc->p_pid);
         proc->tpgid = kproc->p_tpgid;
         proc->session = kproc->p_sid;
         proc->pgrp = kproc->p__pgid;
         proc->isKernelThread = !!(kproc->p_flag & P_SYSTEM);
         proc->isUserlandThread = Process_getPid(proc) != Process_getThreadGroup(proc); // eh?
         proc->starttime_ctime = kproc->p_ustart_sec;
         Process_fillStarttimeBuffer(proc);
         ProcessTable_add(super, proc);

         proc->tty_nr = kproc->p_tdev;
         // KERN_PROC_TTY_NODEV is a negative constant but the type of
         // kproc->p_tdev may be unsigned.
         const char* name = ((dev_t)~kproc->p_tdev != (dev_t)~(KERN_PROC_TTY_NODEV)) ? devname(kproc->p_tdev, S_IFCHR) : NULL;
         if (!name) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }

         NetBSDProcessTable_updateExe(kproc, proc);
         NetBSDProcessTable_updateProcessName(nhost->kd, kproc, proc);
      } else {
         if (settings->updateProcessNames) {
            NetBSDProcessTable_updateProcessName(nhost->kd, kproc, proc);
         }
      }

      if (settings->ss->flags & PROCESS_FLAG_CWD) {
         NetBSDProcessTable_updateCwd(kproc, proc);
      }

      if (proc->st_uid != kproc->p_uid) {
         proc->st_uid = kproc->p_uid;
         proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);
      }

      proc->m_virt = kproc->p_vm_vsize;
      proc->m_resident = kproc->p_vm_rssize;

      proc->percent_mem = (proc->m_resident * nhost->pageSizeKB) / (double)(host->totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(nhost, kproc), 0.0, host->activeCPUs * 100.0);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->nlwp = kproc->p_nlwps;
      proc->nice = kproc->p_nice - 20;
      proc->time = 100 * (kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000));
      proc->priority = kproc->p_priority - PZERO;
      proc->processor = kproc->p_cpuid;
      proc->minflt = kproc->p_uru_minflt;
      proc->majflt = kproc->p_uru_majflt;

      int nlwps = 0;
      const struct kinfo_lwp* klwps = kvm_getlwps(nhost->kd, kproc->p_pid, kproc->p_paddr, sizeof(struct kinfo_lwp), &nlwps);

      /* TODO: According to the link below, SDYING should be a regarded state */
      /* Taken from: https://ftp.netbsd.org/pub/NetBSD/NetBSD-current/src/sys/sys/proc.h */
      switch (kproc->p_realstat) {
         case SIDL:     proc->state = IDLE; break;
         case SACTIVE:
            // We only consider the first LWP with a one of the below states.
            for (int j = 0; j < nlwps; j++) {
               if (klwps) {
                  switch (klwps[j].l_stat) {
                     case LSONPROC: proc->state = RUNNING; break;
                     case LSRUN:    proc->state = RUNNABLE; break;
                     case LSSLEEP:  proc->state = SLEEPING; break;
                     case LSSTOP:   proc->state = STOPPED; break;
                     default:       proc->state = UNKNOWN;
                  }

                  if (proc->state != UNKNOWN) {
                     break;
                  }
               } else {
                  proc->state = UNKNOWN;
                  break;
               }
            }
            break;
         case SSTOP:    proc->state = STOPPED; break;
         case SZOMB:    proc->state = ZOMBIE; break;
         case SDEAD:    proc->state = DEFUNCT; break;
         default:       proc->state = UNKNOWN;
      }

      if (Process_isKernelThread(proc)) {
         super->kernelThreads++;
      } else if (Process_isUserlandThread(proc)) {
         super->userlandThreads++;
      }

      super->totalTasks++;
      if (proc->state == RUNNING) {
         super->runningTasks++;
      }
      proc->super.updated = true;
   }
}
