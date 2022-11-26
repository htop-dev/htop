#ifndef HEADER_DiskUsage
#define HEADER_DiskUsage
/*
htop - DiskUsageMeter.h
(C) 2021 htop dev team
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

#include <math.h>


typedef struct DiskUsageData_ {
   double total;           /* total disk size in bytes */
   double used;            /* used disk space in bytes */
   double usedPercentage;  /* used disk amount in percent */
} DiskUsageData;

extern const DiskUsageData invalidDiskUsageData;

static inline bool isInvalidDiskUsageData(const DiskUsageData *data)
{
   return isnan(data->total);
}

extern const MeterClass DiskUsageMeter_class;

#endif
