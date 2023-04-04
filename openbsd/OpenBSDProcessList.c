/*
htop - OpenBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "openbsd/OpenBSDProcessList.h"

#include <kvm.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/swap.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uvm/uvmexp.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "Settings.h"
#include "XUtils.h"
#include "openbsd/OpenBSDProcess.h"


static long fscale;
static int pageSize;
static int pageSizeKB;

static void OpenBSDProcessList_updateCPUcount(ProcessList* super) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) super;
   const int nmib[] = { CTL_HW, HW_NCPU };
   const int mib[] = { CTL_HW, HW_NCPUONLINE };
   int r;
   unsigned int value;
   size_t size;
   bool change = false;

   size = sizeof(value);
   r = sysctl(mib, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = 1;
   }

   if (value != super->activeCPUs) {
      super->activeCPUs = value;
      change = true;
   }

   size = sizeof(value);
   r = sysctl(nmib, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = super->activeCPUs;
   }

   if (value != super->existingCPUs) {
      opl->cpuData = xReallocArray(opl->cpuData, value + 1, sizeof(CPUData));
      super->existingCPUs = value;
      change = true;
   }

   if (change) {
      CPUData* dAvg = &opl->cpuData[0];
      memset(dAvg, '\0', sizeof(CPUData));
      dAvg->totalTime = 1;
      dAvg->totalPeriod = 1;
      dAvg->online = true;

      for (unsigned int i = 0; i < super->existingCPUs; i++) {
         CPUData* d = &opl->cpuData[i + 1];
         memset(d, '\0', sizeof(CPUData));
         d->totalTime = 1;
         d->totalPeriod = 1;

         const int ncmib[] = { CTL_KERN, KERN_CPUSTATS, i };
         struct cpustats cpu_stats;

         size = sizeof(cpu_stats);
         if (sysctl(ncmib, 3, &cpu_stats, &size, NULL, 0) < 0) {
            CRT_fatalError("ncmib sysctl call failed");
         }
         d->online = (cpu_stats.cs_flags & CPUSTATS_ONLINE);
      }
   }
}


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   const int fmib[] = { CTL_KERN, KERN_FSCALE };
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   OpenBSDProcessList* opl = xCalloc(1, sizeof(OpenBSDProcessList));
   ProcessList* pl = (ProcessList*) opl;
   ProcessList_init(pl, Class(OpenBSDProcess), usersTable, pidMatchList, userId);

   OpenBSDProcessList_updateCPUcount(pl);

   size = sizeof(fscale);
   if (sysctl(fmib, 2, &fscale, &size, NULL, 0) < 0) {
      CRT_fatalError("fscale sysctl call failed");
   }

   if ((pageSize = sysconf(_SC_PAGESIZE)) == -1)
      CRT_fatalError("pagesize sysconf call failed");
   pageSizeKB = pageSize / ONE_K;

   opl->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (opl->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   opl->cpuSpeed = -1;

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) this;

   if (opl->kd) {
      kvm_close(opl->kd);
   }

   free(opl->cpuData);

   ProcessList_done(this);
   free(this);
}

static void OpenBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   const int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      CRT_fatalError("uvmexp sysctl call failed");
   }

   pl->totalMem = uvmexp.npages * pageSizeKB;
   pl->usedMem = (uvmexp.npages - uvmexp.free - uvmexp.paging) * pageSizeKB;

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   const int bcache_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);

   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      CRT_fatalError("cannot get vfs.bcachestat");
   }

   pl->cachedMem = bcstats.numbufpages * pageSizeKB;

   /*
    * Copyright (c) 1994 Thorsten Lockert <tholo@sigmasoft.com>
    * All rights reserved.
    *
    * Taken almost directly from OpenBSD's top(1)
    *
    * Originally released under a BSD-3 license
    * Modified through htop developers applying GPL-2
    */
   int nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap > 0) {
      struct swapent swdev[nswap];
      int rnswap = swapctl(SWAP_STATS, swdev, nswap);

      /* Total things up */
      unsigned long long int total = 0, used = 0;
      for (int i = 0; i < rnswap; i++) {
         if (swdev[i].se_flags & SWF_ENABLE) {
            used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
            total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
         }
      }

      pl->totalSwap = total;
      pl->usedSwap = used;
   } else {
      pl->totalSwap = pl->usedSwap = 0;
   }
}

static void OpenBSDProcessList_updateCwd(const struct kinfo_proc* kproc, Process* proc) {
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

static void OpenBSDProcessList_updateProcessName(kvm_t* kd, const struct kinfo_proc* kproc, Process* proc) {
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
 * Taken from OpenBSD's ps(1).
 */
static double getpcpu(const struct kinfo_proc* kp) {
   if (fscale == 0)
      return 0.0;

   return 100.0 * (double)kp->p_pctcpu / fscale;
}

static void OpenBSDProcessList_scanProcs(OpenBSDProcessList* this) {
   const Settings* settings = this->super.settings;
   const bool hideKernelThreads = settings->hideKernelThreads;
   const bool hideUserlandThreads = settings->hideUserlandThreads;
   int count = 0;

   const struct kinfo_proc* kprocs = kvm_getprocs(this->kd, KERN_PROC_KTHREAD | KERN_PROC_SHOW_THREADS, 0, sizeof(struct kinfo_proc), &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];

      /* Ignore main threads */
      if (kproc->p_tid != -1) {
         Process* containingProcess = ProcessList_findProcess(&this->super, kproc->p_pid);
         if (containingProcess) {
            if (((OpenBSDProcess*)containingProcess)->addr == kproc->p_addr)
               continue;

            containingProcess->nlwp++;
         }
      }

      bool preExisting = false;
      Process* proc = ProcessList_getProcess(&this->super, (kproc->p_tid == -1) ? kproc->p_pid : kproc->p_tid, &preExisting, OpenBSDProcess_new);
      OpenBSDProcess* fp = (OpenBSDProcess*) proc;

      if (!preExisting) {
         proc->ppid = kproc->p_ppid;
         proc->tpgid = kproc->p_tpgid;
         proc->tgid = kproc->p_pid;
         proc->session = kproc->p_sid;
         proc->pgrp = kproc->p__pgid;
         proc->isKernelThread = proc->pgrp == 0;
         proc->isUserlandThread = kproc->p_tid != -1;
         proc->starttime_ctime = kproc->p_ustart_sec;
         Process_fillStarttimeBuffer(proc);
         ProcessList_add(&this->super, proc);

         OpenBSDProcessList_updateProcessName(this->kd, kproc, proc);

         if (settings->ss->flags & PROCESS_FLAG_CWD) {
            OpenBSDProcessList_updateCwd(kproc, proc);
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
            OpenBSDProcessList_updateProcessName(this->kd, kproc, proc);
         }
      }

      fp->addr = kproc->p_addr;
      proc->m_virt = kproc->p_vm_dsize * pageSizeKB;
      proc->m_resident = kproc->p_vm_rssize * pageSizeKB;

      proc->percent_mem = proc->m_resident / (float)this->super.totalMem * 100.0F;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0F, this->super.activeCPUs * 100.0F);
      Process_updateCPUFieldWidths(proc->percent_cpu);

      proc->nice = kproc->p_nice - 20;
      proc->time = 100 * (kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000));
      proc->priority = kproc->p_priority - PZERO;
      proc->processor = kproc->p_cpuid;
      proc->minflt = kproc->p_uru_minflt;
      proc->majflt = kproc->p_uru_majflt;
      proc->nlwp = 1;

      if (proc->st_uid != kproc->p_uid) {
         proc->st_uid = kproc->p_uid;
         proc->user = UsersTable_getRef(this->super.usersTable, proc->st_uid);
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

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));
      proc->updated = true;
   }
}

static void getKernelCPUTimes(unsigned int cpuId, u_int64_t* times) {
   const int mib[] = { CTL_KERN, KERN_CPTIME2, cpuId };
   size_t length = sizeof(*times) * CPUSTATES;
   if (sysctl(mib, 3, times, &length, NULL, 0) == -1 || length != sizeof(*times) * CPUSTATES) {
      CRT_fatalError("sysctl kern.cp_time2 failed");
   }
}

static void kernelCPUTimesToHtop(const u_int64_t* times, CPUData* cpu) {
   unsigned long long totalTime = 0;
   for (int i = 0; i < CPUSTATES; i++) {
      totalTime += times[i];
   }

   unsigned long long sysAllTime = times[CP_INTR] + times[CP_SYS];

   // XXX Not sure if CP_SPIN should be added to sysAllTime.
   // See https://github.com/openbsd/src/commit/531d8034253fb82282f0f353c086e9ad827e031c
   #ifdef CP_SPIN
   sysAllTime += times[CP_SPIN];
   #endif

   cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
   cpu->userPeriod = saturatingSub(times[CP_USER], cpu->userTime);
   cpu->nicePeriod = saturatingSub(times[CP_NICE], cpu->niceTime);
   cpu->sysPeriod = saturatingSub(times[CP_SYS], cpu->sysTime);
   cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
   #ifdef CP_SPIN
   cpu->spinPeriod = saturatingSub(times[CP_SPIN], cpu->spinTime);
   #endif
   cpu->intrPeriod = saturatingSub(times[CP_INTR], cpu->intrTime);
   cpu->idlePeriod = saturatingSub(times[CP_IDLE], cpu->idleTime);

   cpu->totalTime = totalTime;
   cpu->userTime = times[CP_USER];
   cpu->niceTime = times[CP_NICE];
   cpu->sysTime = times[CP_SYS];
   cpu->sysAllTime = sysAllTime;
   #ifdef CP_SPIN
   cpu->spinTime = times[CP_SPIN];
   #endif
   cpu->intrTime = times[CP_INTR];
   cpu->idleTime = times[CP_IDLE];
}

static void OpenBSDProcessList_scanCPUTime(OpenBSDProcessList* this) {
   u_int64_t kernelTimes[CPUSTATES] = {0};
   u_int64_t avg[CPUSTATES] = {0};

   for (unsigned int i = 0; i < this->super.existingCPUs; i++) {
      CPUData* cpu = &this->cpuData[i + 1];

      if (!cpu->online) {
         continue;
      }

      getKernelCPUTimes(i, kernelTimes);
      kernelCPUTimesToHtop(kernelTimes, cpu);

      avg[CP_USER] += cpu->userTime;
      avg[CP_NICE] += cpu->niceTime;
      avg[CP_SYS] += cpu->sysTime;
      #ifdef CP_SPIN
      avg[CP_SPIN] += cpu->spinTime;
      #endif
      avg[CP_INTR] += cpu->intrTime;
      avg[CP_IDLE] += cpu->idleTime;
   }

   for (int i = 0; i < CPUSTATES; i++) {
      avg[i] /= this->super.activeCPUs;
   }

   kernelCPUTimesToHtop(avg, &this->cpuData[0]);

   {
      const int mib[] = { CTL_HW, HW_CPUSPEED };
      int cpuSpeed;
      size_t size = sizeof(cpuSpeed);
      if (sysctl(mib, 2, &cpuSpeed, &size, NULL, 0) == -1) {
         this->cpuSpeed = -1;
      } else {
         this->cpuSpeed = cpuSpeed;
      }
   }
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) super;

   OpenBSDProcessList_updateCPUcount(super);
   OpenBSDProcessList_scanMemoryInfo(super);
   OpenBSDProcessList_scanCPUTime(opl);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   OpenBSDProcessList_scanProcs(opl);
}

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id) {
   assert(id < super->existingCPUs);

   const OpenBSDProcessList* opl = (const OpenBSDProcessList*) super;
   return opl->cpuData[id + 1].online;
}
