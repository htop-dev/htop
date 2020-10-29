#ifndef HEADER_DiskIOMeter
#define HEADER_DiskIOMeter
/*
 h top - DiskIOMeter*.h
(C) 2020 Christian Göttsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef struct DiskIOData_ {
   unsigned long int totalBytesRead;
   unsigned long int totalBytesWritten;
   unsigned long int totalMsTimeSpend;
} DiskIOData;

extern const MeterClass DiskIOMeter_class;

#endif /* HEADER_DiskIOMeter */
