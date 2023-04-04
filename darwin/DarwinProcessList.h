#ifndef HEADER_DarwinProcessList
#define HEADER_DarwinProcessList
/*
htop - DarwinProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include "ProcessList.h"
#include "zfs/ZfsArcStats.h"


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

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate);

bool ProcessList_isCPUonline(const ProcessList* super, unsigned int id);

#endif
