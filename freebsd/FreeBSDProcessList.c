/*
htop - FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "FreeBSDProcessList.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <fcntl.h>

/*{

#include <kvm.h>

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int totalPeriod;
} CPUData;

typedef struct FreeBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   CPUData* cpus;

} FreeBSDProcessList;

}*/

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_cache_count[4];
static int MIB_hw_physmem[2];

static int pageSizeKb;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   FreeBSDProcessList* fpl = calloc(1, sizeof(FreeBSDProcessList));
   ProcessList* pl = (ProcessList*) fpl;
   ProcessList_init(pl, usersTable, pidWhiteList, userId);

   int cpus = 1;
   size_t sizeof_cpus = sizeof(cpus);
   int err = sysctlbyname("hw.ncpu", &cpus, &sizeof_cpus, NULL, 0);
   if (err) cpus = 1;
   pl->cpuCount = MAX(cpus, 1);
   fpl->cpus = realloc(fpl->cpus, cpus * sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      fpl->cpus[i].totalTime = 1;
      fpl->cpus[i].totalPeriod = 1;
   }
   
   size_t len;
   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count",  MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", MIB_vm_stats_vm_v_cache_count, &len);
   len = 2; sysctlnametomib("hw.physmem",                MIB_hw_physmem, &len);
   pageSizeKb = PAGE_SIZE_KB;   
   
   fpl->kd = kvm_open(NULL, "/dev/null", NULL, 0, NULL);
   assert(fpl->kd);

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;
   if (fpl->kd) kvm_close(fpl->kd);
  
   ProcessList_done(this);
   free(this);
}

static inline void FreeBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;
   
   size_t len = sizeof(pl->totalMem);
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(pl->usedMem), &len, NULL, 0);
   pl->usedMem *= pageSizeKb;
   pl->freeMem = pl->totalMem - pl->usedMem;
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(pl->cachedMem), &len, NULL, 0);
   pl->cachedMem *= pageSizeKb;
   
   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(fpl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= pageSizeKb;
   pl->usedSwap *= pageSizeKb;
   
   pl->sharedMem = 0;  // currently unused
   pl->buffersMem = 0; // not exposed to userspace
}

void ProcessList_scan(ProcessList* this) {
   (void) this;

   FreeBSDProcessList_scanMemoryInfo(this);

   // stub!
}
