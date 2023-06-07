#ifndef HEADER_ZramMeter
#define HEADER_ZramMeter

#include "Meter.h"

typedef enum {
   ZRAM_METER_COMPRESSED = 0,
   ZRAM_METER_UNCOMPRESSED = 1,
   ZRAM_METER_ITEMCOUNT = 2, // number of entries in this enum
} ZramMeterValues;

extern const MeterClass ZramMeter_class;

#endif
