/*
htop - FreeBSDMachine.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "freebsd/FreeBSDMachine.h"

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
#include "Scheduling.h"
#include "Settings.h"
#include "XUtils.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


static int MIB_hw_physmem[2];
static int MIB_vm_stats_vm_v_page_count[4];

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_active_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_vm_stats_vm_v_inactive_count[4];
static int MIB_vm_stats_vm_v_free_count[4];
static int MIB_vm_vmtotal[2];

static int MIB_vfs_bufspace[2];

static int MIB_kern_cp_time[2];
static int MIB_kern_cp_times[2];

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   FreeBSDMachine* this = xCalloc(1, sizeof(FreeBSDMachine));
   Machine* super = &this->super;
   char errbuf[_POSIX2_LINE_MAX];
   size_t len;

   Machine_init(super, usersTable, userId);

   // physical memory in system: hw.physmem
   // physical page size: hw.pagesize
   // usable pagesize : vm.stats.vm.v_page_size
   len = 2; sysctlnametomib("hw.physmem", MIB_hw_physmem, &len);

   len = sizeof(this->pageSize);
   if (sysctlbyname("vm.stats.vm.v_page_size", &this->pageSize, &len, NULL, 0) == -1)
      CRT_fatalError("Cannot get pagesize by sysctl");
   this->pageSizeKb = this->pageSize / ONE_K;

   // usable page count vm.stats.vm.v_page_count
   // actually usable memory : vm.stats.vm.v_page_count * vm.stats.vm.v_page_size
   len = 4; sysctlnametomib("vm.stats.vm.v_page_count", MIB_vm_stats_vm_v_page_count, &len);

   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count", MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_active_count", MIB_vm_stats_vm_v_active_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", MIB_vm_stats_vm_v_cache_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_inactive_count", MIB_vm_stats_vm_v_inactive_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_free_count", MIB_vm_stats_vm_v_free_count, &len);
   len = 2; sysctlnametomib("vm.vmtotal", MIB_vm_vmtotal, &len);

   len = 2; sysctlnametomib("vfs.bufspace", MIB_vfs_bufspace, &len);

   openzfs_sysctl_init(&this->zfs);
   openzfs_sysctl_updateArcStats(&this->zfs);

   int smp = 0;
   len = sizeof(smp);

   if (sysctlbyname("kern.smp.active", &smp, &len, NULL, 0) != 0 || len != sizeof(smp)) {
      smp = 0;
   }

   int cpus = 1;
   len = sizeof(cpus);

   if (smp) {
      int err = sysctlbyname("kern.smp.cpus", &cpus, &len, NULL, 0);
      if (err) {
         cpus = 1;
      }
   } else {
      cpus = 1;
   }

   size_t sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   len = 2; sysctlnametomib("kern.cp_time", MIB_kern_cp_time, &len);
   this->cp_time_o = xCalloc(CPUSTATES, sizeof(unsigned long));
   this->cp_time_n = xCalloc(CPUSTATES, sizeof(unsigned long));
   len = sizeof_cp_time_array;

   // fetch initial single (or average) CPU clicks from kernel
   sysctl(MIB_kern_cp_time, 2, this->cp_time_o, &len, NULL, 0);

   // on smp box, fetch rest of initial CPU's clicks
   if (cpus > 1) {
      len = 2; sysctlnametomib("kern.cp_times", MIB_kern_cp_times, &len);
      this->cp_times_o = xCalloc(cpus, sizeof_cp_time_array);
      this->cp_times_n = xCalloc(cpus, sizeof_cp_time_array);
      len = cpus * sizeof_cp_time_array;
      sysctl(MIB_kern_cp_times, 2, this->cp_times_o, &len, NULL, 0);
   }

   super->existingCPUs = MAXIMUM(cpus, 1);
   // TODO: support offline CPUs and hot swapping
   super->activeCPUs = super->existingCPUs;

   if (cpus == 1 ) {
      this->cpus = xRealloc(this->cpus, sizeof(CPUData));
   } else {
      // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
      this->cpus = xRealloc(this->cpus, (super->existingCPUs + 1) * sizeof(CPUData));
   }

   len = sizeof(this->kernelFScale);
   if (sysctlbyname("kern.fscale", &this->kernelFScale, &len, NULL, 0) == -1 || this->kernelFScale <= 0) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      this->kernelFScale = 2048;
   }

   this->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (this->kd == NULL) {
      CRT_fatalError("kvm_openfiles() failed");
   }

   return super;
}

void Machine_delete(Machine* super) {
   FreeBSDMachine* this = (FreeBSDMachine*) super;

   Machine_done(super);

   if (this->kd) {
      kvm_close(this->kd);
   }

   free(this->cp_time_o);
   free(this->cp_time_n);
   free(this->cp_times_o);
   free(this->cp_times_n);
   free(this->cpus);

   free(this);
}

static inline void FreeBSDMachine_scanCPU(Machine* super) {
   const FreeBSDMachine* this = (FreeBSDMachine*) super;

   unsigned int cpus   = super->existingCPUs; // actual CPU count
   unsigned int maxcpu = cpus;             // max iteration (in case we have average + smp)
   int cp_times_offset;

   assert(cpus > 0);

   size_t sizeof_cp_time_array;

   unsigned long* cp_time_n; // old clicks state
   unsigned long* cp_time_o; // current clicks state

   unsigned long cp_time_d[CPUSTATES];
   double        cp_time_p[CPUSTATES];

   // get averages or single CPU clicks
   sizeof_cp_time_array = sizeof(unsigned long) * CPUSTATES;
   sysctl(MIB_kern_cp_time, 2, this->cp_time_n, &sizeof_cp_time_array, NULL, 0);

   // get rest of CPUs
   if (cpus > 1) {
      // on smp systems FreeBSD kernel concats all CPU states into one long array in
      // kern.cp_times sysctl OID
      // we store averages in this->cpus[0], and actual cores after that
      maxcpu = cpus + 1;
      sizeof_cp_time_array = cpus * sizeof(unsigned long) * CPUSTATES;
      sysctl(MIB_kern_cp_times, 2, this->cp_times_n, &sizeof_cp_time_array, NULL, 0);
   }

   for (unsigned int i = 0; i < maxcpu; i++) {
      if (cpus == 1) {
         // single CPU box
         cp_time_n = this->cp_time_n;
         cp_time_o = this->cp_time_o;
      } else {
         if (i == 0 ) {
            // average
            cp_time_n = this->cp_time_n;
            cp_time_o = this->cp_time_o;
         } else {
            // specific smp cores
            cp_times_offset = i - 1;
            cp_time_n = this->cp_times_n + (cp_times_offset * CPUSTATES);
            cp_time_o = this->cp_times_o + (cp_times_offset * CPUSTATES);
         }
      }

      // diff old vs new
      unsigned long long total_o = 0;
      unsigned long long total_n = 0;
      unsigned long long total_d = 0;
      for (int s = 0; s < CPUSTATES; s++) {
         cp_time_d[s] = cp_time_n[s] - cp_time_o[s];
         total_o += cp_time_o[s];
         total_n += cp_time_n[s];
      }

      // totals
      total_d = total_n - total_o;
      if (total_d < 1 ) {
         total_d = 1;
      }

      // save current state as old and calc percentages
      for (int s = 0; s < CPUSTATES; ++s) {
         cp_time_o[s] = cp_time_n[s];
         cp_time_p[s] = ((double)cp_time_d[s]) / ((double)total_d) * 100;
      }

      CPUData* cpuData = &(this->cpus[i]);
      cpuData->userPercent      = cp_time_p[CP_USER];
      cpuData->nicePercent      = cp_time_p[CP_NICE];
      cpuData->systemPercent    = cp_time_p[CP_SYS];
      cpuData->irqPercent       = cp_time_p[CP_INTR];
      cpuData->systemAllPercent = cp_time_p[CP_SYS] + cp_time_p[CP_INTR];
      // this one is not really used
      //cpuData->idlePercent      = cp_time_p[CP_IDLE];

      cpuData->temperature = NAN;
      cpuData->frequency = NAN;

      const int coreId = (cpus == 1) ? 0 : ((int)i - 1);
      if (coreId < 0)
         continue;

      // TODO: test with hyperthreading and multi-cpu systems
      if (super->settings->showCPUTemperature) {
         int temperature;
         size_t len = sizeof(temperature);
         char mibBuffer[32];
         xSnprintf(mibBuffer, sizeof(mibBuffer), "dev.cpu.%d.temperature", coreId);
         int r = sysctlbyname(mibBuffer, &temperature, &len, NULL, 0);
         if (r == 0)
            cpuData->temperature = (double)(temperature - 2732) / 10.0; // convert from deci-Kelvin to Celsius
      }

      // TODO: test with hyperthreading and multi-cpu systems
      if (super->settings->showCPUFrequency) {
         int frequency;
         size_t len = sizeof(frequency);
         char mibBuffer[32];
         xSnprintf(mibBuffer, sizeof(mibBuffer), "dev.cpu.%d.freq", coreId);
         int r = sysctlbyname(mibBuffer, &frequency, &len, NULL, 0);
         if (r == 0)
            cpuData->frequency = frequency; // keep in MHz
      }
   }

   // calculate max temperature and avg frequency for average meter and
   // propagate frequency to all cores if only supplied for CPU 0
   if (cpus > 1) {
      if (super->settings->showCPUTemperature) {
         double maxTemp = -HUGE_VAL;
         for (unsigned int i = 1; i < maxcpu; i++) {
            if (isgreater(this->cpus[i].temperature, maxTemp)) {
               maxTemp = this->cpus[i].temperature;
               this->cpus[0].temperature = maxTemp;
            }
         }
      }

      if (super->settings->showCPUFrequency) {
         const double coreZeroFreq = this->cpus[1].frequency;
         double freqSum = coreZeroFreq;
         if (isNonnegative(coreZeroFreq)) {
            for (unsigned int i = 2; i < maxcpu; i++) {
               if (!isNonnegative(this->cpus[i].frequency))
                  this->cpus[i].frequency = coreZeroFreq;

               freqSum += this->cpus[i].frequency;
            }

            this->cpus[0].frequency = freqSum / (maxcpu - 1);
         }
      }
   }
}

static void FreeBSDMachine_scanMemoryInfo(Machine* super) {
   FreeBSDMachine* this = (FreeBSDMachine*) super;

   // @etosan:
   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired
   //  htop_cache (for cache meter)   = buffers + cache
   //  user_free (avail to procs)     = buffers + inactive + cache + free
   //
   // with ZFS ARC situation becomes bit muddled, as ARC behaves like "user_free"
   // and belongs into cache, but is reported as wired by kernel
   //
   // htop_used   = active + (wired - arc)
   // htop_cache  = buffers + cache + arc
   u_long totalMem;
   u_int memActive, memWire, cachedMem;
   long buffersMem;
   size_t len;
   struct vmtotal vmtotal;

   //disabled for now, as it is always smaller than phycal amount of memory...
   //...to avoid "where is my memory?" questions
   //sysctl(MIB_vm_stats_vm_v_page_count, 4, &(super->totalMem), &len, NULL, 0);
   //super->totalMem *= this->pageSizeKb;
   len = sizeof(totalMem);
   sysctl(MIB_hw_physmem, 2, &(totalMem), &len, NULL, 0);
   totalMem /= 1024;
   super->totalMem = totalMem;

   len = sizeof(memActive);
   sysctl(MIB_vm_stats_vm_v_active_count, 4, &(memActive), &len, NULL, 0);
   memActive *= this->pageSizeKb;
   this->memActive = memActive;

   len = sizeof(memWire);
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(memWire), &len, NULL, 0);
   memWire *= this->pageSizeKb;
   this->memWire = memWire;

   len = sizeof(buffersMem);
   sysctl(MIB_vfs_bufspace, 2, &(buffersMem), &len, NULL, 0);
   buffersMem /= 1024;
   super->buffersMem = buffersMem;

   len = sizeof(cachedMem);
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(cachedMem), &len, NULL, 0);
   cachedMem *= this->pageSizeKb;
   super->cachedMem = cachedMem;

   len = sizeof(vmtotal);
   sysctl(MIB_vm_vmtotal, 2, &(vmtotal), &len, NULL, 0);
   super->sharedMem = vmtotal.t_rmshr * this->pageSizeKb;

   super->usedMem = this->memActive + this->memWire;

   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(this->kd, swap, ARRAYSIZE(swap), 0);
   super->totalSwap = 0;
   super->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      super->totalSwap += swap[i].ksw_total;
      super->usedSwap += swap[i].ksw_used;
   }
   super->totalSwap *= this->pageSizeKb;
   super->usedSwap *= this->pageSizeKb;
}

void Machine_scan(Machine* super) {
   FreeBSDMachine* this = (FreeBSDMachine*) super;

   openzfs_sysctl_updateArcStats(&this->zfs);
   FreeBSDMachine_scanMemoryInfo(super);
   FreeBSDMachine_scanCPU(super);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);

   // TODO: support offline CPUs and hot swapping
   (void) host; (void) id;

   return true;
}
