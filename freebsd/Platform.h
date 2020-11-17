#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - freebsd/Platform.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

extern ProcessField Platform_defaultFields[];

extern int Platform_numberOfFields;

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

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

char* Platform_getInodeFilename(pid_t pid, ino_t inode);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(unsigned long int* bytesReceived,
                           unsigned long int* packetsReceived,
                           unsigned long int* bytesTransmitted,
                           unsigned long int* packetsTransmitted);

void Platform_getBattery(double* level, ACPresence* isOnAC);

#endif
