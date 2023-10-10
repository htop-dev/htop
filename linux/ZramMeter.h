#ifndef HEADER_ZramMeter
#define HEADER_ZramMeter
/*
htop - linux/ZramMeter.h
(C) 2020 Murloc Knight
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef enum {
   ZRAM_METER_COMPRESSED = 0,
   ZRAM_METER_UNCOMPRESSED = 1,
   ZRAM_METER_ITEMCOUNT = 2, // number of entries in this enum
} ZramMeterValues;

extern const MeterClass ZramMeter_class;

#endif
