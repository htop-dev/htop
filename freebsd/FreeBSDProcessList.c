/*
htop - FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "freebsd/FreeBSDProcessList.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_iovec.h>
#include <sys/errno.h>
#include <sys/param.h> // needs to be included before <sys/jail.h> for MAXPATHLEN
#include <sys/jail.h>
#include <sys/priority.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include "CRT.h"
#include "Compat.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "Scheduling.h"
#include "Settings.h"
#include "XUtils.h"

#include "freebsd/FreeBSDMachine.h"
#include "freebsd/FreeBSDProcess.h"


ProcessList* ProcessList_new(Machine* host, Hashtable* pidMatchList) {
   FreeBSDProcessList* this = xCalloc(1, sizeof(FreeBSDProcessList));
   Object_setClass(this, Class(ProcessList));

   ProcessList* super = &this->super;
   ProcessList_init(super, Class(FreeBSDProcess), host, pidMatchList);

   return super;
}

void ProcessList_delete(Object* cast) {
   FreeBSDProcessList* this = (FreeBSDProcessList*) cast;
   ProcessList_done(&this->super);
   free(this);
}

static void FreeBSDProcessList_updateExe(const struct kinfo_proc* kproc, Process* proc) {
   if (Process_isKernelThread(proc)) {
      Process_updateExe(proc, NULL);
      return;
   }

   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, kproc->ki_pid };
   char buffer[2048];
   size_t size = sizeof(buffer);
   if (sysctl(mib, 4, buffer, &size, NULL, 0) != 0) {
      Process_updateExe(proc, NULL);
      return;
   }

   Process_updateExe(proc, buffer);
}

static void FreeBSDProcessList_updateCwd(const struct kinfo_proc* kproc, Process* proc) {
#ifdef KERN_PROC_CWD
   const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_CWD, kproc->ki_pid };
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
#else
   proc->procCwd = NULL;
#endif
}

static void FreeBSDProcessList_updateProcessName(kvm_t* kd, const struct kinfo_proc* kproc, Process* proc) {
   Process_updateComm(proc, kproc->ki_comm);

   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv || !argv[0]) {
      Process_updateCmdline(proc, kproc->ki_comm, 0, strlen(kproc->ki_comm));
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

static char* FreeBSDProcessList_readJailName(const struct kinfo_proc* kproc) {
   if (kproc->ki_jid == 0)
      return xStrdup("-");

   char jnamebuf[MAXHOSTNAMELEN] = {0};
   struct iovec jiov[4];

IGNORE_WCASTQUAL_BEGIN
   *(const void**)&jiov[0].iov_base = "jid";
   jiov[0].iov_len = sizeof("jid");
   jiov[1].iov_base = (void*) &kproc->ki_jid;
   jiov[1].iov_len = sizeof(kproc->ki_jid);
   *(const void**)&jiov[2].iov_base = "name";
   jiov[2].iov_len = sizeof("name");
   jiov[3].iov_base = jnamebuf;
   jiov[3].iov_len = sizeof(jnamebuf);
IGNORE_WCASTQUAL_END

   int jid = jail_get(jiov, 4, 0);
   if (jid == kproc->ki_jid)
      return xStrdup(jnamebuf);

   return NULL;
}

void ProcessList_goThroughEntries(ProcessList* super) {
   const Machine* host = super->host;
   const FreeBSDMachine* fhost = (const FreeBSDMachine*) host;
   const Settings* settings = host->settings;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   int count = 0;
   const struct kinfo_proc* kprocs = kvm_getprocs(fhost->kd, KERN_PROC_PROC, 0, &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting = false;
      Process* proc = ProcessList_getProcess(super, kproc->ki_pid, &preExisting, FreeBSDProcess_new);
      FreeBSDProcess* fp = (FreeBSDProcess*) proc;

      if (!preExisting) {
         fp->jid = kproc->ki_jid;
         Process_setPid(proc, kproc->ki_pid);
         Process_setThreadGroup(proc, kproc->ki_pid);
         Process_setParent(proc, kproc->ki_ppid);
         proc->isKernelThread = kproc->ki_pid != 1 && (kproc->ki_flag & P_SYSTEM);
         proc->isUserlandThread = false;
         proc->tpgid = kproc->ki_tpgid;
         proc->session = kproc->ki_sid;
         proc->pgrp = kproc->ki_pgid;
         proc->st_uid = kproc->ki_uid;
         proc->starttime_ctime = kproc->ki_start.tv_sec;
         if (proc->starttime_ctime < 0) {
            proc->starttime_ctime = host->realtimeMs / 1000;
         }
         Process_fillStarttimeBuffer(proc);
         proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);
         ProcessList_add(super, proc);

         FreeBSDProcessList_updateExe(kproc, proc);
         FreeBSDProcessList_updateProcessName(fhost->kd, kproc, proc);

         if (settings->ss->flags & PROCESS_FLAG_CWD) {
            FreeBSDProcessList_updateCwd(kproc, proc);
         }

         fp->jname = FreeBSDProcessList_readJailName(kproc);

         proc->tty_nr = kproc->ki_tdev;
         const char* name = (kproc->ki_tdev != NODEV) ? devname(kproc->ki_tdev, S_IFCHR) : NULL;
         if (!name) {
            free(proc->tty_name);
            proc->tty_name = NULL;
         } else {
            free_and_xStrdup(&proc->tty_name, name);
         }
      } else {
         if (fp->jid != kproc->ki_jid) {
            // process can enter jail anytime
            fp->jid = kproc->ki_jid;
            free(fp->jname);
            fp->jname = FreeBSDProcessList_readJailName(kproc);
         }
         // if there are reapers in the system, process can get reparented anytime
         proc->ppid = kproc->ki_ppid;
         if (proc->st_uid != kproc->ki_uid) {
            // some processes change users (eg. to lower privs)
            proc->st_uid = kproc->ki_uid;
            proc->user = UsersTable_getRef(host->usersTable, proc->st_uid);
         }
         if (settings->updateProcessNames) {
            FreeBSDProcessList_updateProcessName(fhost->kd, kproc, proc);
         }
      }

      free_and_xStrdup(&fp->emul, kproc->ki_emul);

      // from FreeBSD source /src/usr.bin/top/machine.c
      proc->m_virt = kproc->ki_size / ONE_K;
      proc->m_resident = kproc->ki_rssize * fhost->pageSizeKb;
      proc->nlwp = kproc->ki_numthreads;
      proc->time = (kproc->ki_runtime + 5000) / 10000;

      proc->percent_cpu = 100.0 * ((double)kproc->ki_pctcpu / (double)fhost->kernelFScale);
      proc->percent_mem = 100.0 * proc->m_resident / (double)(host->totalMem);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      if (kproc->ki_stat == SRUN && kproc->ki_oncpu != NOCPU) {
         proc->processor = kproc->ki_oncpu;
      } else {
         proc->processor = kproc->ki_lastcpu;
      }

      proc->majflt = kproc->ki_cow;

      proc->priority = kproc->ki_pri.pri_level - PZERO;

      if (String_eq("intr", kproc->ki_comm) && (kproc->ki_flag & P_SYSTEM)) {
         proc->nice = 0; //@etosan: intr kernel process (not thread) has weird nice value
      } else if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
         proc->nice = kproc->ki_nice - NZERO;
      } else if (PRI_IS_REALTIME(kproc->ki_pri.pri_class)) {
         proc->nice = PRIO_MIN - 1 - (PRI_MAX_REALTIME - kproc->ki_pri.pri_level);
      } else {
         proc->nice = PRIO_MAX + 1 + kproc->ki_pri.pri_level - PRI_MIN_IDLE;
      }

      /* Taken from: https://github.com/freebsd/freebsd-src/blob/1ad2d87778970582854082bcedd2df0394fd4933/sys/sys/proc.h#L851 */
      switch (kproc->ki_stat) {
      case SIDL:   proc->state = IDLE; break;
      case SRUN:   proc->state = RUNNING; break;
      case SSLEEP: proc->state = SLEEPING; break;
      case SSTOP:  proc->state = STOPPED; break;
      case SZOMB:  proc->state = ZOMBIE; break;
      case SWAIT:  proc->state = WAITING; break;
      case SLOCK:  proc->state = BLOCKED; break;
      default:     proc->state = UNKNOWN;
      }

      if (Process_isKernelThread(proc))
         super->kernelThreads++;

#ifdef SCHEDULER_SUPPORT
      if (settings->ss->flags & PROCESS_FLAG_SCHEDPOL)
         Scheduling_readProcessPolicy(proc);
#endif

      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      super->totalTasks++;
      if (proc->state == RUNNING)
         super->runningTasks++;
      proc->super.updated = true;
   }
}
