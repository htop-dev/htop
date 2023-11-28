#ifndef HEADER_DiskIOMeter
#define HEADER_DiskIOMeter
/*
htop - DiskIOMeter.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>

#include "Meter.h"


typedef struct DiskIOData_ {
   uint64_t totalBytesRead;
   uint64_t totalBytesWritten;
   uint64_t totalMsTimeSpend;
} DiskIOData;

extern const MeterClass DiskIOMeter_class;

#endif /* HEADER_DiskIOMeter */
