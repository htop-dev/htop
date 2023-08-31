/*
htop - DragonFlyBSDMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "dragonflybsd/DragonFlyBSDMachine.h"

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

#include "dragonflybsd/DragonFlyBSDProcess.h"


static int MIB_hw_physmem[2];
static int MIB_vm_stats_vm_v_page_count[4];

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_active_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_vm_stats_vm_v_inactive_count[4];
static int MIB_vm_stats_vm_v_free_count[4];

static int MIB_vfs_bufspace[2];

static int MIB_kern_cp_time[2];
static int MIB_kern_cp_times[2];

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   size_t len;
   char errbuf[_POSIX2_LINE_MAX];
   DragonFlyBSDMachine* this = xCalloc(1, sizeof(DragonFlyBSDMachine));
   Machine* super = &this->super;

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

   len = 2; sysctlnametomib("vfs.bufspace", MIB_vfs_bufspace, &len);

   int cpus = 1;
   len = sizeof(cpus);
   if (sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0) != 0) {
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
   const DragonFlyBSDMachine* this = (const DragonFlyBSDMachine*) super;

   Machine_done(super);

   if (this->kd) {
      kvm_close(this->kd);
   }

   if (this->jails) {
      Hashtable_delete(this->jails);
   }

   free(this->cp_time_o);
   free(this->cp_time_n);
   free(this->cp_times_o);
   free(this->cp_times_n);
   free(this->cpus);

   free(this);
}

static void DragonFlyBSDMachine_scanCPUTime(Machine* super) {
   const DragonFlyBSDMachine* this = (DragonFlyBSDMachine*) super;

   unsigned int cpus   = super->existingCPUs;  // actual CPU count
   unsigned int maxcpu = cpus;              // max iteration (in case we have average + smp)
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
      // on smp systems DragonFlyBSD kernel concats all CPU states into one long array in
      // kern.cp_times sysctl OID
      // we store averages in dfpl->cpus[0], and actual cores after that
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
      // this one is not really used, but we store it anyway
      cpuData->idlePercent      = cp_time_p[CP_IDLE];
   }
}

static void DragonFlyBSDMachine_scanMemoryInfo(Machine* super) {
   DragonFlyBSDMachine* this = (DragonFlyBSDProcessTable*) super;

   // @etosan:
   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired
   //  htop_cache (for cache meter)   = buffers + cache
   //  user_free (avail to procs)     = buffers + inactive + cache + free
   size_t len = sizeof(super->totalMem);

   //disabled for now, as it is always smaller than phycal amount of memory...
   //...to avoid "where is my memory?" questions
   //sysctl(MIB_vm_stats_vm_v_page_count, 4, &(this->totalMem), &len, NULL, 0);
   //this->totalMem *= pageSizeKb;
   sysctl(MIB_hw_physmem, 2, &(super->totalMem), &len, NULL, 0);
   super->totalMem /= 1024;

   sysctl(MIB_vm_stats_vm_v_active_count, 4, &(this->memActive), &len, NULL, 0);
   this->memActive *= this->pageSizeKb;

   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(this->memWire), &len, NULL, 0);
   this->memWire *= this->pageSizeKb;

   sysctl(MIB_vfs_bufspace, 2, &(super->buffersMem), &len, NULL, 0);
   super->buffersMem /= 1024;

   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(super->cachedMem), &len, NULL, 0);
   super->cachedMem *= this->pageSizeKb;
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

static void DragonFlyBSDMachine_scanJails(DragonFlyBSDMachine* this) {
   size_t len;
   char* jails; /* Jail list */
   char* curpos;
   char* nextpos;

   if (sysctlbyname("jail.list", NULL, &len, NULL, 0) == -1) {
      CRT_fatalError("initial sysctlbyname / jail.list failed");
   }

retry:
   if (len == 0)
      return;

   jails = xMalloc(len);

   if (sysctlbyname("jail.list", jails, &len, NULL, 0) == -1) {
      if (errno == ENOMEM) {
         free(jails);
         goto retry;
      }
      CRT_fatalError("sysctlbyname / jail.list failed");
   }

   if (this->jails) {
      Hashtable_delete(this->jails);
   }

   this->jails = Hashtable_new(20, true);
   curpos = jails;
   while (curpos) {
      int jailid;
      char* str_hostname;

      nextpos = strchr(curpos, '\n');
      if (nextpos) {
         *nextpos++ = 0;
      }

      jailid = atoi(strtok(curpos, " "));
      str_hostname = strtok(NULL, " ");

      char* jname = (char*) (Hashtable_get(this->jails, jailid));
      if (jname == NULL) {
         jname = xStrdup(str_hostname);
         Hashtable_put(this->jails, jailid, jname);
      }

      curpos = nextpos;
   }

   free(jails);
}

char* DragonFlyBSDMachine_readJailName(DragonFlyBSDMachine* host, int jailid) {
   char* hostname;
   char* jname;

   if (jailid != 0 && host->jails && (hostname = (char*)Hashtable_get(host->jails, jailid))) {
      jname = xStrdup(hostname);
   } else {
      jname = xStrdup("-");
   }

   return jname;
}

void Machine_scan(Machine* super) {
   DragonFlyBSDMachine* this = (DragonFlyBSDMachine*) super;

   DragonFlyBSDMachine_scanMemoryInfo(super);
   DragonFlyBSDMachine_scanCPUTime(super);
   DragonFlyBSDMachine_scanJails(this);
}
