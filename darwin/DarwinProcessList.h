#ifndef HEADER_DarwinProcessList
#define HEADER_DarwinProcessList
/*
htop - DarwinProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

struct kern;

void GetKernelVersion(struct kern *k);

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
int CompareKernelVersion(short int major, short int minor, short int component);

#include "ProcessList.h"
#include "zfs/ZfsArcStats.h"
#include <mach/mach_host.h>
#include <sys/sysctl.h>

typedef struct DarwinProcessList_ {
   ProcessList super;

   host_basic_info_data_t host_info;
   vm_statistics_data_t vm_stats;
   processor_cpu_load_info_t prev_load;
   processor_cpu_load_info_t curr_load;
   uint64_t kernel_threads;
   uint64_t user_threads;
   uint64_t global_diff;

   ZfsArcStats zfs;
} DarwinProcessList;

void ProcessList_getHostInfo(host_basic_info_data_t *p);

void ProcessList_freeCPULoadInfo(processor_cpu_load_info_t *p);

unsigned ProcessList_allocateCPULoadInfo(processor_cpu_load_info_t *p);

void ProcessList_getVMStats(vm_statistics_t p);

struct kinfo_proc *ProcessList_getKInfoProcs(size_t *count);

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super);

#endif
