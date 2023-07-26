#ifndef HEADER_DarwinMachine
#define HEADER_DarwinMachine
/*
htop - DarwinMachine.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include "Machine.h"
#include "zfs/ZfsArcStats.h"
#include "CpuFreq.h"

typedef struct DarwinMachine_ {
   Machine super;

   host_basic_info_data_t host_info;
   vm_statistics_data_t vm_stats;
   processor_cpu_load_info_t prev_load;
   processor_cpu_load_info_t curr_load;
#ifdef CPUFREQ_SUPPORT
   CpuFreqData cpu_freq;
   bool cpu_freq_ok;
#endif

   ZfsArcStats zfs;
} DarwinMachine;

#endif
