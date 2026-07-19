#ifndef HEADER_ZswapStats
#define HEADER_ZswapStats
/*
htop - ZswapStats.h
(C) 2022 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "ProcessTable.h"

typedef struct ZswapStats_ {
   /* maximum configured size of the zswap pool */
   memory_t totalZswapPool;
   /* amount of RAM used by the zswap pool */
   memory_t usedZswapComp;
   /* amount of data stored inside the zswap pool */
   memory_t usedZswapOrig;
   /* whether both zswap counters are available from /proc/meminfo */
   bool available;
   /* whether zswap is enabled */
   bool enabled;
   /* whether totalZswapPool was obtained from the zswap module parameter */
   bool hasPoolLimit;
} ZswapStats;

#endif
