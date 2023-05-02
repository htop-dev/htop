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


typedef struct DarwinProcessList_ {
   ProcessList super;

   uint64_t global_diff;
} DarwinProcessList;

#endif
