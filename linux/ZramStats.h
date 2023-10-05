#ifndef HEADER_ZramStats
#define HEADER_ZramStats
/*
htop - ZramStats.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessTable.h"

typedef struct ZramStats_ {
   memory_t totalZram;
   memory_t usedZramComp;
   memory_t usedZramOrig;
} ZramStats;

#endif
