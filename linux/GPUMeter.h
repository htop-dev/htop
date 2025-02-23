#ifndef HEADER_GPUMeter
#define HEADER_GPUMeter
/*
htop - GPUMeter.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Meter.h"


struct GPUMeterEngineData {
   const char* key;  /* owned by LinuxMachine */
   unsigned long long int timeDiff;
   double percentage;
};

extern struct GPUMeterEngineData GPUMeter_engineData[4];

extern const MeterClass GPUMeter_class;

bool GPUMeter_active(void);

#endif /* HEADER_GPUMeter */
