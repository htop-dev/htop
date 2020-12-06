/*
htop - OpenBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "OpenBSDProcessList.h"

#include <err.h>
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
#include "OpenBSDProcess.h"
#include "Process.h"
#include "ProcessList.h"
#include "Settings.h"
#include "XUtils.h"


static long fscale;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   const int mib[] = { CTL_HW, HW_NCPU };
   const int fmib[] = { CTL_KERN, KERN_FSCALE };
   int r;
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   OpenBSDProcessList* opl = xCalloc(1, sizeof(OpenBSDProcessList));
   ProcessList* pl = (ProcessList*) opl;
   ProcessList_init(pl, Class(OpenBSDProcess), usersTable, pidMatchList, userId);

   size = sizeof(pl->cpuCount);
   r = sysctl(mib, 2, &pl->cpuCount, &size, NULL, 0);
   if (r < 0 || pl->cpuCount < 1) {
      pl->cpuCount = 1;
   }
   opl->cpus = xCalloc(pl->cpuCount + 1, sizeof(CPUData));

   size = sizeof(fscale);
   if (sysctl(fmib, 2, &fscale, &size, NULL, 0) < 0) {
      err(1, "fscale sysctl call failed");
   }

   for (int i = 0; i <= pl->cpuCount; i++) {
      CPUData* d = opl->cpus + i;
      d->totalTime = 1;
      d->totalPeriod = 1;
   }

   opl->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (opl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) this;

   if (opl->kd) {
      kvm_close(opl->kd);
   }

   free(opl->cpus);

   ProcessList_done(this);
   free(this);
}

static void OpenBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   const int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      err(1, "uvmexp sysctl call failed");
   }

   pl->totalMem = uvmexp.npages * CRT_pageSizeKB;
   pl->usedMem = (uvmexp.npages - uvmexp.free - uvmexp.paging) * CRT_pageSizeKB;

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   const int bcache_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);

   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      err(1, "cannot get vfs.bcachestat");
   }

   pl->cachedMem = bcstats.numbufpages * CRT_pageSizeKB;

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

static char* OpenBSDProcessList_readProcessName(kvm_t* kd, const struct kinfo_proc* kproc, int* basenameEnd) {
   /*
    * Like OpenBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   char** arg = kvm_getargv(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      return xStrdup(kproc->p_comm);
   }

   size_t len = 0;
   for (int i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }

   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   char* s;
   if ((s = malloc(len)) == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      return xStrdup(kproc->p_comm);
   }

   *s = '\0';

   for (int i = 0; arg[i] != NULL; i++) {
      size_t n = strlcat(s, arg[i], len);
      if (i == 0) {
         *basenameEnd = MINIMUM(n, len - 1);
      }
      /* the trailing space should get truncated anyway */
      strlcat(s, " ", len);
   }

   return s;
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
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
   int count = 0;

   const struct kinfo_proc* kprocs = kvm_getprocs(this->kd, KERN_PROC_KTHREAD, 0, sizeof(struct kinfo_proc), &count);

   for (int i = 0; i < count; i++) {
      const struct kinfo_proc* kproc = &kprocs[i];

      bool preExisting = false;
      Process* proc = ProcessList_getProcess(&this->super, kproc->p_pid, &preExisting, OpenBSDProcess_new);
      //OpenBSDProcess* fp = (OpenBSDProcess*) proc;

      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      if (!preExisting) {
         proc->ppid = kproc->p_ppid;
         proc->tpgid = kproc->p_tpgid;
         proc->tgid = kproc->p_pid;
         proc->session = kproc->p_sid;
         proc->tty_nr = kproc->p_tdev;
         proc->pgrp = kproc->p__pgid;
         proc->st_uid = kproc->p_uid;
         proc->starttime_ctime = kproc->p_ustart_sec;
         Process_fillStarttimeBuffer(proc);
         proc->user = UsersTable_getRef(this->super.usersTable, proc->st_uid);
         ProcessList_add(&this->super, proc);
         proc->comm = OpenBSDProcessList_readProcessName(this->kd, kproc, &proc->basenameOffset);
      } else {
         if (settings->updateProcessNames) {
            free(proc->comm);
            proc->comm = OpenBSDProcessList_readProcessName(this->kd, kproc, &proc->basenameOffset);
         }
      }

      proc->m_virt = kproc->p_vm_dsize;
      proc->m_resident = kproc->p_vm_rssize;
      proc->percent_mem = (proc->m_resident * CRT_pageSizeKB) / (double)(this->super.totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->super.cpuCount * 100.0);
      //proc->nlwp = kproc->p_numthreads;
      proc->nice = kproc->p_nice - 20;
      proc->time = 100 * (kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000));
      proc->priority = kproc->p_priority - PZERO;

      switch (kproc->p_stat) {
         case SIDL:    proc->state = 'I'; break;
         case SRUN:    proc->state = 'R'; break;
         case SSLEEP:  proc->state = 'S'; break;
         case SSTOP:   proc->state = 'T'; break;
         case SZOMB:   proc->state = 'Z'; break;
         case SDEAD:   proc->state = 'D'; break;
         case SONPROC: proc->state = 'P'; break;
         default:      proc->state = '?';
      }

      if (Process_isKernelThread(proc)) {
         this->super.kernelThreads++;
      }

      this->super.totalTasks++;
      // SRUN ('R') means runnable, not running
      if (proc->state == 'P') {
         this->super.runningTasks++;
      }
      proc->updated = true;
   }
}

static unsigned long long saturatingSub(unsigned long long a, unsigned long long b) {
   return a > b ? a - b : 0;
}

static void getKernelCPUTimes(int cpuId, u_int64_t* times) {
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

   // XXX Not sure if CP_SPIN should be added to sysAllTime.
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

   for (int i = 0; i < this->super.cpuCount; i++) {
      getKernelCPUTimes(i, kernelTimes);
      CPUData* cpu = this->cpus + i + 1;
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
      avg[i] /= this->super.cpuCount;
   }

   kernelCPUTimesToHtop(avg, this->cpus);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) super;

   OpenBSDProcessList_scanMemoryInfo(super);
   OpenBSDProcessList_scanCPUTime(opl);

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   OpenBSDProcessList_scanProcs(opl);
}
