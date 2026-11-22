#ifndef HEADER_NetworkIOMeter
#define HEADER_NetworkIOMeter
/*
htop - NetworkIOMeter.h
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Meter.h"


/* Maximum interface name length on Linux, without the terminating NUL */
#define NETWORKIO_INTERFACE_NAME_MAX 15

typedef struct NetworkIOData_ {
   uint64_t bytesReceived;
   uint64_t packetsReceived;
   uint64_t bytesTransmitted;
   uint64_t packetsTransmitted;
} NetworkIOData;

typedef struct NetworkInterfaceIOData_ {
   char name[NETWORKIO_INTERFACE_NAME_MAX + 1];
   NetworkIOData data;
   uint32_t linkSpeedMbps;
} NetworkInterfaceIOData;

/* A non-NULL array from Platform_getNetworkIO indicates per-interface support,
   including when its element count is zero. The caller owns the array. */

extern const MeterClass NetworkIOMeter_class;

bool NetworkIOMeter_isValidInterfaceName(const char* name);

/*
 * Per-interface meters store the interface by name: Meter.param indexes a
 * process wide table of names, 0 is the aggregate over all interfaces.
 * Returns 0 if the name is invalid.
 */
unsigned int NetworkIOMeter_paramForInterface(const char* name);

/* NULL for the aggregate meter */
const char* NetworkIOMeter_interfaceForParam(size_t param);

#endif /* HEADER_NetworkIOMeter */
