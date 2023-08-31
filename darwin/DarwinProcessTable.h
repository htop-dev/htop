#ifndef HEADER_DarwinProcessTable
#define HEADER_DarwinProcessTable
/*
htop - DarwinProcessTable.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include "ProcessTable.h"


typedef struct DarwinProcessTable_ {
   ProcessTable super;

   uint64_t global_diff;
} DarwinProcessTable;

#endif
