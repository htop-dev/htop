#ifndef HEADER_BatteryMeter
#define HEADER_BatteryMeter
/*
htop - BatteryMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "Meter.h"

typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;

extern int BatteryMeter_attributes[];

extern MeterClass BatteryMeter_class;

#endif
