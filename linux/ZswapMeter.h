#ifndef HEADER_ZswapMeter
#define HEADER_ZswapMeter
/*
htop - ZswapMeter.h
(C) 2026 Abhiram Shibu
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"


typedef enum {
   ZSWAP_METER_COMPRESSED = 0,
   ZSWAP_METER_ITEMCOUNT = 1, // number of entries in this enum
} ZswapMeterValues;

typedef enum {
   ZSWAP_STATS_METER_COMPRESSED = 0,
   ZSWAP_STATS_METER_ORIGINAL = 1,
   ZSWAP_STATS_METER_ITEMCOUNT = 2, // number of entries in this enum
} ZswapStatsMeterValues;

extern const MeterClass ZswapMeter_class;

extern const MeterClass ZswapStatsMeter_class;

#endif
