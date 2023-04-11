#ifndef HEADER_MemoryMeter
#define HEADER_MemoryMeter
/*
htop - MemoryMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef enum {
   MEMORY_METER_USED = 0,
   MEMORY_METER_BUFFERS = 1,
   MEMORY_METER_SHARED = 2,
   MEMORY_METER_CACHE = 3,
   MEMORY_METER_AVAILABLE = 4,
   MEMORY_METER_ITEMCOUNT = 5, // number of entries in this enum
} MemoryMeterValues;

extern const MeterClass MemoryMeter_class;

#endif
