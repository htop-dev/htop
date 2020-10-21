#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - openbsd/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

extern ProcessField Platform_defaultFields[];

extern int Platform_numberOfFields;

/* see /usr/include/sys/signal.h */
extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

void Platform_setBindings(Htop_Action* keys);

extern const MeterClass* const Platform_meterTypes[];

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(unsigned long int *bytesReceived,
                           unsigned long int *packetsReceived,
                           unsigned long int *bytesTransmitted,
                           unsigned long int *packetsTransmitted);

#endif
