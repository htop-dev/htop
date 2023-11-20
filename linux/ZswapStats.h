#ifndef HEADER_ZswapStats
#define HEADER_ZswapStats
/*
htop - ZswapStats.h
(C) 2022 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessTable.h"

typedef struct ZswapStats_ {
   /* amount of RAM used by the zswap pool */
   memory_t usedZswapComp;
   /* amount of data stored inside the zswap pool */
   memory_t usedZswapOrig;
} ZswapStats;

#endif
