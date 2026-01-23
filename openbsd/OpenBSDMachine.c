/*
htop - OpenBSDMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "openbsd/OpenBSDMachine.h"

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
#include "Settings.h"
#include "XUtils.h"


static void OpenBSDMachine_updateCPUcount(OpenBSDMachine* this) {
   Machine* super = &this->super;
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
      this->cpuData = xReallocArray(this->cpuData, value + 1, sizeof(CPUData));
      super->existingCPUs = value;
      change = true;
   }

   if (change) {
      CPUData* dAvg = &this->cpuData[0];
      memset(dAvg, '\0', sizeof(CPUData));
      dAvg->totalTime = 1;
      dAvg->totalPeriod = 1;
      dAvg->online = true;

      for (unsigned int i = 0; i < super->existingCPUs; i++) {
         CPUData* d = &this->cpuData[i + 1];
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

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   const int fmib[] = { CTL_KERN, KERN_FSCALE };
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   OpenBSDMachine* this = xCalloc(1, sizeof(OpenBSDMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   OpenBSDMachine_updateCPUcount(this);

   size = sizeof(this->fscale);
   if (sysctl(fmib, 2, &this->fscale, &size, NULL, 0) < 0 || this->fscale <= 0) {
      CRT_fatalError("fscale sysctl call failed");
   }

   long pageSize = sysconf(_SC_PAGESIZE);
   if (pageSize <= 0)
      CRT_fatalError("pagesize sysconf call failed");
   this->pageSize = (size_t)pageSize;
   this->pageSizeKB = this->pageSize / ONE_K;

   this->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (this->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   this->cpuSpeed = -1;

   return super;
}

void Machine_delete(Machine* super) {
   OpenBSDMachine* this = (OpenBSDMachine*) super;

   if (this->kd) {
      kvm_close(this->kd);
   }
   free(this->cpuData);
   Machine_done(super);
   free(this);
}

static void OpenBSDMachine_scanMemoryInfo(OpenBSDMachine* this) {
   Machine* super = &this->super;
   const int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      CRT_fatalError("uvmexp sysctl call failed");
   }

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   const int bcache_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);
   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      CRT_fatalError("cannot get vfs.bcachestat");
   }

   // NOTE: in OpenBSD the "cached" memory is a subset of the "wired" memory.
   super->totalMem   = this->pageSizeKB * uvmexp.npages;
   this->wiredMem    = this->pageSizeKB * (uvmexp.npages - uvmexp.free - uvmexp.active - uvmexp.paging - bcstats.numbufpages); // NB: uvmexp.wired == 0!? deduct it
   this->cacheMem    = this->pageSizeKB * bcstats.numbufpages;
   this->activeMem   = this->pageSizeKB * uvmexp.active;
   this->pagingMem   = this->pageSizeKB * uvmexp.paging;
   this->inactiveMem = this->pageSizeKB * uvmexp.inactive;

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
      struct swapent* swdev = xMallocArray(nswap, sizeof(struct swapent));
      int rnswap = swapctl(SWAP_STATS, swdev, nswap);

      /* Total things up */
      unsigned long long int total = 0, used = 0;
      for (int i = 0; i < rnswap; i++) {
         if (swdev[i].se_flags & SWF_ENABLE) {
            used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
            total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
         }
      }

      super->totalSwap = total;
      super->usedSwap = used;

      free(swdev);
   } else {
      super->totalSwap = super->usedSwap = 0;
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

static void OpenBSDMachine_scanCPUTime(OpenBSDMachine* this) {
   Machine* super = &this->super;
   u_int64_t kernelTimes[CPUSTATES] = {0};
   u_int64_t avg[CPUSTATES] = {0};

   for (unsigned int i = 0; i < super->existingCPUs; i++) {
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
      avg[i] /= super->activeCPUs;
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

void Machine_scan(Machine* super) {
   OpenBSDMachine* this = (OpenBSDMachine*) super;

   OpenBSDMachine_updateCPUcount(this);
   OpenBSDMachine_scanMemoryInfo(this);
   OpenBSDMachine_scanCPUTime(this);
}

bool Machine_isCPUonline(const Machine* super, unsigned int id) {
   assert(id < super->existingCPUs);

   const OpenBSDMachine* this = (const OpenBSDMachine*) super;
   return this->cpuData[id + 1].online;
}
