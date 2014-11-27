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

typedef struct FreeBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;
} FreeBSDProcessList;

}*/

static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_hw_physmem[2];

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   FreeBSDProcessList* this = calloc(1, sizeof(FreeBSDProcessList));
   ProcessList* pl = (ProcessList*) this;
   ProcessList_init((ProcessList*)this, usersTable, pidWhiteList);

   int cpus = 1;
   size_t sizeof_cpus = sizeof(cpus);
   int err = sysctlbyname("hw.ncpu", &cpus, &sizeof_cpus, NULL, 0);
   if (err) cpus = 1;
   pl->cpuCount = MAX(cpus, 1);
   pl->cpus = realloc(pl->cpus, cpus * sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      pl->cpus[i].totalTime = 1;
      pl->cpus[i].totalPeriod = 1;
   }
   
   size_t len = 4;
   sysctlnametomib("vm.stats.vm.v_wire_count", MIB_vm_stats_vm_v_wire_count, &len);
   len = 2;
   sysctlnametomib("hw.physmem", MIB_hw_physmem, &len);

   return (ProcessList*) this;
}

static inline void FreeBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;
   
   unsigned long long int swapFree = 0;
   size_t len = sizeof(pl->totalMem);
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(pl->usedMem), &len, NULL, 0);
   pl->usedMem *= PAGE_SIZE / 1024;
   pl->freeMem = pl->totalMem - pl->usedMem;
   
   pl->sharedMem = 0;
   pl->buffersMem = 0;
   pl->cachedMem = 0;
   pl->totalSwap = 0;
   swapFree = 0;
   pl->usedSwap = pl->totalSwap - swapFree;
}

void ProcessList_scan(ProcessList* this) {
   (void) this;

   FreeBSDProcessList_scanMemoryInfo(this);

   // stub!
}
