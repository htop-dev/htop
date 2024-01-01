/*
htop - NetBSDMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "netbsd/NetBSDMachine.h"

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
#include <sys/swap.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uvm/uvm_extern.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Settings.h"
#include "XUtils.h"


static const struct {
   const char* name;
   long int scale;
} freqSysctls[] = {
   { "machdep.est.frequency.current",            1 },
   { "machdep.powernow.frequency.current",       1 },
   { "machdep.intrepid.frequency.current",       1 },
   { "machdep.loongson.frequency.current",       1 },
   { "machdep.cpu.frequency.current",            1 },
   { "machdep.frequency.current",                1 },
   { "machdep.tsc_freq",                   1000000 },
};

static void NetBSDMachine_updateCPUcount(NetBSDMachine* this) {
   Machine* super = &this->super;

   // Definitions for sysctl(3), cf. https://nxr.netbsd.org/xref/src/sys/sys/sysctl.h#813
   const int mib_ncpu_existing[] = { CTL_HW, HW_NCPU }; // Number of existing CPUs
   const int mib_ncpu_online[] = { CTL_HW, HW_NCPUONLINE }; // Number of online/active CPUs

   int r;
   unsigned int value;
   size_t size;

   bool change = false;

   // Query the number of active/online CPUs.
   size = sizeof(value);
   r = sysctl(mib_ncpu_online, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = 1;
   }

   if (value != super->activeCPUs) {
      super->activeCPUs = value;
      change = true;
   }

   // Query the total number of CPUs.
   size = sizeof(value);
   r = sysctl(mib_ncpu_existing, 2, &value, &size, NULL, 0);
   if (r < 0 || value < 1) {
      value = super->activeCPUs;
   }

   if (value != super->existingCPUs) {
      this->cpuData = xReallocArray(this->cpuData, value + 1, sizeof(CPUData));
      super->existingCPUs = value;
      change = true;
   }

   // Reset CPU stats when number of online/existing CPU cores changed
   if (change) {
      CPUData* dAvg = &this->cpuData[0];
      memset(dAvg, '\0', sizeof(CPUData));
      dAvg->totalTime = 1;
      dAvg->totalPeriod = 1;

      for (unsigned int i = 0; i < super->existingCPUs; i++) {
         CPUData* d = &this->cpuData[i + 1];
         memset(d, '\0', sizeof(CPUData));
         d->totalTime = 1;
         d->totalPeriod = 1;
      }
   }
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   const int fmib[] = { CTL_KERN, KERN_FSCALE };
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   NetBSDMachine* this = xCalloc(1, sizeof(NetBSDMachine));
   Machine* super = &this->super;
   Machine_init(super, usersTable, userId);

   NetBSDMachine_updateCPUcount(this);

   size = sizeof(this->fscale);
   if (sysctl(fmib, 2, &this->fscale, &size, NULL, 0) < 0 || this->fscale <= 0) {
      CRT_fatalError("fscale sysctl call failed");
   }

   if ((this->pageSize = sysconf(_SC_PAGESIZE)) == -1)
      CRT_fatalError("pagesize sysconf call failed");
   this->pageSizeKB = this->pageSize / ONE_K;

   this->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (this->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   return super;
}

void Machine_delete(Machine* super) {
   NetBSDMachine* this = (NetBSDMachine*) super;

   Machine_done(super);

   if (this->kd) {
      kvm_close(this->kd);
   }
   free(this->cpuData);
   free(this);
}

static void NetBSDMachine_scanMemoryInfo(NetBSDMachine* this) {
   Machine* super = &this->super;

   static int uvmexp_mib[] = {CTL_VM, VM_UVMEXP2};
   struct uvmexp_sysctl uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      CRT_fatalError("uvmexp sysctl call failed");
   }

   super->totalMem = uvmexp.npages * this->pageSizeKB;
   super->buffersMem = 0;
   super->cachedMem = (uvmexp.filepages + uvmexp.execpages) * this->pageSizeKB;
   super->usedMem = (uvmexp.active + uvmexp.wired) * this->pageSizeKB;
   super->totalSwap = uvmexp.swpages * this->pageSizeKB;
   super->usedSwap = uvmexp.swpginuse * this->pageSizeKB;
}

static void getKernelCPUTimes(int cpuId, u_int64_t* times) {
   const int mib[] = { CTL_KERN, KERN_CP_TIME, cpuId };
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

   cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
   cpu->userPeriod = saturatingSub(times[CP_USER], cpu->userTime);
   cpu->nicePeriod = saturatingSub(times[CP_NICE], cpu->niceTime);
   cpu->sysPeriod = saturatingSub(times[CP_SYS], cpu->sysTime);
   cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
   cpu->intrPeriod = saturatingSub(times[CP_INTR], cpu->intrTime);
   cpu->idlePeriod = saturatingSub(times[CP_IDLE], cpu->idleTime);

   cpu->totalTime = totalTime;
   cpu->userTime = times[CP_USER];
   cpu->niceTime = times[CP_NICE];
   cpu->sysTime = times[CP_SYS];
   cpu->sysAllTime = sysAllTime;
   cpu->intrTime = times[CP_INTR];
   cpu->idleTime = times[CP_IDLE];
}

static void NetBSDMachine_scanCPUTime(NetBSDMachine* this) {
   const Machine* super = &this->super;

   u_int64_t kernelTimes[CPUSTATES] = {0};
   u_int64_t avg[CPUSTATES] = {0};

   for (unsigned int i = 0; i < super->existingCPUs; i++) {
      getKernelCPUTimes(i, kernelTimes);
      CPUData* cpu = &this->cpuData[i + 1];
      kernelCPUTimesToHtop(kernelTimes, cpu);

      avg[CP_USER] += cpu->userTime;
      avg[CP_NICE] += cpu->niceTime;
      avg[CP_SYS] += cpu->sysTime;
      avg[CP_INTR] += cpu->intrTime;
      avg[CP_IDLE] += cpu->idleTime;
   }

   for (int i = 0; i < CPUSTATES; i++) {
      avg[i] /= super->activeCPUs;
   }

   kernelCPUTimesToHtop(avg, &this->cpuData[0]);
}

static void NetBSDMachine_scanCPUFrequency(NetBSDMachine* this) {
   const Machine* super = &this->super;
   unsigned int cpus = super->existingCPUs;
   bool match = false;
   char name[64];
   long int freq = 0;
   size_t freqSize;

   for (unsigned int i = 0; i < cpus; i++) {
      this->cpuData[i + 1].frequency = NAN;
   }

   /* newer hardware supports per-core frequency, for e.g. ARM big.LITTLE */
   for (unsigned int i = 0; i < cpus; i++) {
      xSnprintf(name, sizeof(name), "machdep.cpufreq.cpu%u.current", i);
      freqSize = sizeof(freq);
      if (sysctlbyname(name, &freq, &freqSize, NULL, 0) != -1) {
         this->cpuData[i + 1].frequency = freq; /* already in MHz */
         match = true;
      }
   }

   if (match) {
      return;
   }

   /*
    * Iterate through legacy sysctl nodes for single-core frequency until
    * we find a match...
    */
   for (size_t i = 0; i < ARRAYSIZE(freqSysctls); i++) {
      freqSize = sizeof(freq);
      if (sysctlbyname(freqSysctls[i].name, &freq, &freqSize, NULL, 0) != -1) {
         freq /= freqSysctls[i].scale; /* scale to MHz */
         match = true;
         break;
      }
   }

   if (match) {
      for (unsigned int i = 0; i < cpus; i++) {
         this->cpuData[i + 1].frequency = freq;
      }
   }
}

void Machine_scan(Machine* super) {
   NetBSDMachine* this = (NetBSDMachine*) super;

   NetBSDMachine_scanMemoryInfo(this);
   NetBSDMachine_scanCPUTime(this);

   if (super->settings->showCPUFrequency) {
      NetBSDMachine_scanCPUFrequency(this);
   }
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);
   (void)host; (void)id;

   // TODO: Support detecting online / offline CPUs.
   return true;
}
