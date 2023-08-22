/*
htop - DragonFlyBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "dragonflybsd/DragonFlyBSDProcessList.h"

#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/param.h>

#include "CRT.h"
#include "Macros.h"

#include "dragonflybsd/DragonFlyBSDMachine.h"
#include "dragonflybsd/DragonFlyBSDProcess.h"


ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList) {
   DragonFlyBSDProcessList* this = xCalloc(1, sizeof(DragonFlyBSDProcessList));
   Object_setClass(this, Class(ProcessList));

   ProcessList* super = (ProcessList*) this;
   ProcessList_init(super, Class(DragonFlyBSDProcess), host, pidMatchList);

   return super;
}

void ProcessList_delete(Object* cast) {
   const DragonFlyBSDProcessList* this = (DragonFlyBSDProcessList*) cast;
   ProcessList_done(&this->super);
   free(this);
}

//static void DragonFlyBSDProcessList_updateExe(const struct kinfo_proc* kproc, Process* proc) {
//   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, kproc->kp_pid };
//   char buffer[2048];
//   size_t size = sizeof(buffer);
//   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
//      Process_updateExe(proc, NULL);
//      return;
//   }
//
//   /* Kernel threads return an empty buffer */
//   if (buffer[0] == '\0') {
//      Process_updateExe(proc, NULL);
//      return;
//   }
//
//   Process_updateExe(proc, buffer);
//}

static void DragonFlyBSDProcessList_updateExe(const struct kinfo_proc* kproc, Process* proc) {
   if (Process_isKernelThread(proc))
      return;

   char path[32];
   xSnprintf(path, sizeof(path), "/proc/%d/file", kproc->kp_pid);

   char target[PATH_MAX];
   ssize_t ret = readlink(path, target, sizeof(target) - 1);
   if (ret <= 0)
      return;

   target[ret] = '\0';
   Process_updateExe(proc, target);
}

static void DragonFlyBSDProcessList_updateCwd(const struct kinfo_proc* kproc, Process* proc) {
   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_CWD, kproc->kp_pid };
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

static void DragonFlyBSDProcessList_updateProcessName(kvm_t* kd, const struct kinfo_proc* kproc, Process* proc) {
   Process_updateComm(proc, kproc->kp_comm);

   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv || !argv[0]) {
      Process_updateCmdline(proc, kproc->kp_comm, 0, strlen(kproc->kp_comm));
      return;
   }

   size_t len = 0;
   for (int i = 0; argv[i]; i++) {
      len += strlen(argv[i]) + 1;
   }

   char* cmdline = xMalloc(len);

   char* at = cmdline;
   int end = 0;
   for (int i = 0; argv[i]; i++) {
      at = stpcpy(at, argv[i]);
      if (end == 0) {
         end = at - cmdline;
      }
      *at++ = ' ';
   }
   at--;
   *at = '\0';

   Process_updateCmdline(proc, cmdline, 0, end);

   free(cmdline);
}

void ProcessList_goThroughEntries(ProcessList* super) {
   const Machine* host = super->host;
   const DragonFlyMachine* dhost = (const DragonFlyMachine*) host;
   const Settings* settings = host->settings;

   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   int count = 0;

   const struct kinfo_proc* kprocs = kvm_getprocs(dhost->kd, KERN_PROC_ALL | (!hideUserlandThreads ? KERN_PROC_FLAG_LWP : 0), 0, &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting = false;
      bool ATTR_UNUSED isIdleProcess = false;

      // note: dragonflybsd kernel processes all have the same pid, so we misuse the kernel thread address to give them a unique identifier
      Process* proc = ProcessList_getProcess(super, kproc->kp_ktaddr ? (pid_t)kproc->kp_ktaddr : kproc->kp_pid, &preExisting, DragonFlyBSDProcess_new);
      DragonFlyBSDProcess* dfp = (DragonFlyBSDProcess*) proc;

      if (!preExisting) {
         dfp->jid = kproc->kp_jailid;
         if (kproc->kp_ktaddr && kproc->kp_flags & P_SYSTEM) {
            // dfb kernel threads all have the same pid, so we misuse the kernel thread address to give them a unique identifier
            Process_setPid(proc, (pid_t)kproc->kp_ktaddr);
            proc->isKernelThread = true;
         } else {
            Process_setPid(proc, kproc->kp_pid);		// process ID
            proc->isKernelThread = false;
         }
         proc->isUserlandThread = kproc->kp_nthreads > 1;
         Process_setParent(proc, kproc->kp_ppid); // parent process id
         proc->tpgid = kproc->kp_tpgid;		// tty process group id
         //Process_setThreadGroup(proc, kproc->kp_lwp.kl_tid);	// thread group id
         Process_setThreadGroup(proc, kproc->kp_pid);
         proc->pgrp = kproc->kp_pgid;		// process group id
         proc->session = kproc->kp_sid;
         proc->st_uid = kproc->kp_uid;		// user ID
         proc->processor = kproc->kp_lwp.kl_origcpu;
         proc->starttime_ctime = kproc->kp_start.tv_sec;
         Process_fillStarttimeBuffer(proc);
         proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);

         proc->tty_nr = kproc->kp_tdev; // control terminal device number
         const char* name = (kproc->kp_tdev != NODEV) ? devname(kproc->kp_tdev, S_IFCHR) : NULL;
         if (!name) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }

         DragonFlyBSDProcessList_updateExe(kproc, proc);
         DragonFlyBSDProcessList_updateProcessName(dhost->kd, kproc, proc);

         if (settings->ss->flags & PROCESS_FLAG_CWD) {
            DragonFlyBSDProcessList_updateCwd(kproc, proc);
         }

         ProcessList_add(super, proc);

         dfp->jname = DragonFlyBSDMachine_readJailName(dhost, kproc->kp_jailid);
      } else {
         proc->processor = kproc->kp_lwp.kl_cpuid;
         if (dfp->jid != kproc->kp_jailid) {	// process can enter jail anytime
            dfp->jid = kproc->kp_jailid;
            free(dfp->jname);
            dfp->jname = DragonFlyBSDMachine_readJailName(dhost, kproc->kp_jailid);
         }
         // if there are reapers in the system, process can get reparented anytime
         Process_setParent(proc, kproc->kp_ppid);
         if (proc->st_uid != kproc->kp_uid) {	// some processes change users (eg. to lower privs)
            proc->st_uid = kproc->kp_uid;
            proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);
         }
         if (settings->updateProcessNames) {
            DragonFlyBSDProcessList_updateProcessName(dhost->kd, kproc, proc);
         }
      }

      proc->m_virt = kproc->kp_vm_map_size / ONE_K;
      proc->m_resident = kproc->kp_vm_rssize * dhost->pageSizeKb;
      proc->nlwp = kproc->kp_nthreads;		// number of lwp thread
      proc->time = (kproc->kp_lwp.kl_uticks + kproc->kp_lwp.kl_sticks + kproc->kp_lwp.kl_iticks) / 10000;

      proc->percent_cpu = 100.0 * ((double)kproc->kp_lwp.kl_pctcpu / (double)dhost->kernelFScale);
      proc->percent_mem = 100.0 * proc->m_resident / (double)(super->totalMem);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      if (proc->percent_cpu > 0.1) {
         // system idle process should own all CPU time left regardless of CPU count
         if (String_eq("idle", kproc->kp_comm)) {
            isIdleProcess = true;
         }
      }

      if (kproc->kp_lwp.kl_pid != -1)
         proc->priority = kproc->kp_lwp.kl_prio;
      else
         proc->priority = -kproc->kp_lwp.kl_tdprio;

      switch (kproc->kp_lwp.kl_rtprio.type) {
         case RTP_PRIO_REALTIME:
            proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX + kproc->kp_lwp.kl_rtprio.prio;
            break;
         case RTP_PRIO_IDLE:
            proc->nice = PRIO_MAX + 1 + kproc->kp_lwp.kl_rtprio.prio;
            break;
         case RTP_PRIO_THREAD:
            proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX - kproc->kp_lwp.kl_rtprio.prio;
            break;
         default:
            proc->nice = kproc->kp_nice;
            break;
      }

      // would be nice if we could store multiple states in proc->state (as enum) and have writeField render them
      /* Taken from: https://github.com/DragonFlyBSD/DragonFlyBSD/blob/c163a4d7ee9c6857ee4e04a3a2cbb50c3de29da1/sys/sys/proc_common.h */
      switch (kproc->kp_stat) {
      case SIDL:   proc->state = IDLE; isIdleProcess = true; break;
      case SACTIVE:
         switch (kproc->kp_lwp.kl_stat) {
            case LSSLEEP:
               if (kproc->kp_lwp.kl_flags & LWP_SINTR)					// interruptible wait short/long
                  if (kproc->kp_lwp.kl_slptime >= MAXSLP) {
                     proc->state = IDLE;
                     isIdleProcess = true;
                  } else {
                     proc->state = SLEEPING;
                  }
               else if (kproc->kp_lwp.kl_tdflags & TDF_SINTR)				// interruptible lwkt wait
                  proc->state = SLEEPING;
               else if (kproc->kp_paddr)						// uninterruptible wait
                  proc->state = UNINTERRUPTIBLE_WAIT;
               else									// uninterruptible lwkt wait
                  proc->state = UNINTERRUPTIBLE_WAIT;
               break;
            case LSRUN:
               if (kproc->kp_lwp.kl_stat == LSRUN) {
                  if (!(kproc->kp_lwp.kl_tdflags & (TDF_RUNNING | TDF_RUNQ)))
                     proc->state = QUEUED;
                  else
                     proc->state = RUNNING;
               }
               break;
            case LSSTOP:
               proc->state = STOPPED;
               break;
            default:
               proc->state = PAGING;
               break;
         }
         break;
      case SSTOP:  proc->state = STOPPED; break;
      case SZOMB:  proc->state = ZOMBIE; break;
      case SCORE:  proc->state = BLOCKED; break;
      default:     proc->state = UNKNOWN;
      }

      if (kproc->kp_flags & P_SWAPPEDOUT)
         proc->state = SLEEPING;
      if (kproc->kp_flags & P_TRACED)
         proc->state = TRACED;
      if (kproc->kp_flags & P_JAILED)
         proc->state = TRACED;

      if (Process_isKernelThread(proc))
         super->kernelThreads++;

      super->totalTasks++;

      if (proc->state == RUNNING)
         super->runningTasks++;

      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));
      proc->super.updated = true;
   }
}
