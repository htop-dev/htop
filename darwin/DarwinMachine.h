#ifndef HEADER_DarwinMachine
#define HEADER_DarwinMachine
/*
htop - DarwinMachine.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <IOKit/IOKitLib.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include "Machine.h"
#include "zfs/ZfsArcStats.h"


typedef struct DarwinMachine_ {
   Machine super;

   host_basic_info_data_t host_info;
   vm_statistics64_data_t vm_stats;
   processor_cpu_load_info_t prev_load;
   processor_cpu_load_info_t curr_load;

   io_service_t GPUService;

   ZfsArcStats zfs;
} DarwinMachine;

#endif
