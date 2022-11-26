#ifndef HEADER_NetworkIOMeter
#define HEADER_NetworkIOMeter

#include "Meter.h"


typedef struct NetworkIOData_ {
   uint64_t bytesReceived;
   uint64_t packetsReceived;
   uint64_t bytesTransmitted;
   uint64_t packetsTransmitted;
} NetworkIOData;

extern const MeterClass NetworkIOMeter_class;
extern const MeterClass NetworkInterfaceIOMeter_class;

#endif /* HEADER_NetworkIOMeter */
