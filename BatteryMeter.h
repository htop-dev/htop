#ifndef HEADER_BatteryMeter
#define HEADER_BatteryMeter
/*
htop - BatteryMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "Meter.h"


typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;

typedef struct BatteryInfo_ {
   ACPresence ac;

   double percent;          /* [0..100], NAN if unknown */
   double powerCurr;        /* instantaneous power in W, NAN if unknown.
                             * Sign convention: positive while discharging
                             * (drawing from the battery), negative while
                             * charging.  Platforms that cannot determine the
                             * direction (e.g. PCP/denki, which exposes only a
                             * magnitude) publish a non-negative best-effort
                             * reading. */
   double energyCurr;       /* Wh, NAN if unknown */
   double energyFull;       /* Wh, NAN if unknown */
} BatteryInfo;

extern const MeterClass BatteryMeter_class;

#endif
