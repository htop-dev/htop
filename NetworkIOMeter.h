#ifndef HEADER_NetworkIOMeter
#define HEADER_NetworkIOMeter

#include "Meter.h"

typedef struct NetworkIOData_ {
   unsigned long long int bytesReceived;
   unsigned long long int packetsReceived;
   unsigned long long int bytesTransmitted;
   unsigned long long int packetsTransmitted;
} NetworkIOData;

extern const MeterClass NetworkIOMeter_class;

#endif /* HEADER_NetworkIOMeter */
