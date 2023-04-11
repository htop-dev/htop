#ifndef HEADER_SwapMeter
#define HEADER_SwapMeter
/*
htop - SwapMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef enum {
   SWAP_METER_USED = 0,
   SWAP_METER_CACHE = 1,
   SWAP_METER_ITEMCOUNT = 2, // number of entries in this enum
} SwapMeterValues;

extern const MeterClass SwapMeter_class;

#endif
