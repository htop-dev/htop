#ifndef HEADER_CPUMeter
#define HEADER_CPUMeter
/*
htop - CPUMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef enum {
   CPU_METER_NICE = 0,
   CPU_METER_NORMAL = 1,
   CPU_METER_KERNEL = 2,
   CPU_METER_IRQ = 3,
   CPU_METER_SOFTIRQ = 4,
   CPU_METER_STEAL = 5,
   CPU_METER_GUEST = 6,
   CPU_METER_IOWAIT = 7,
   CPU_METER_FREQUENCY = 8,
   CPU_METER_ITEMCOUNT = 9, // number of entries in this enum
} CPUMeterValues;

extern int CPUMeter_attributes[];

extern MeterClass CPUMeter_class;

extern MeterClass AllCPUsMeter_class;

extern MeterClass AllCPUs2Meter_class;

extern MeterClass LeftCPUsMeter_class;

extern MeterClass RightCPUsMeter_class;

extern MeterClass LeftCPUs2Meter_class;

extern MeterClass RightCPUs2Meter_class;

extern MeterClass AllCPUs4Meter_class;

extern MeterClass LeftCPUs4Meter_class;

extern MeterClass RightCPUs4Meter_class;

#endif
