#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - unsupported/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "UnsupportedProcess.h"

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern ProcessField Platform_defaultFields[];

extern ProcessFieldData Process_fields[];

extern const MeterClass* const Platform_meterTypes[];

void Platform_setBindings(Htop_Action* keys);

extern int Platform_numberOfFields;

extern char Process_pidFormat[20];

extern ProcessPidColumn Process_pidColumns[];

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

bool Process_isThread(const Process* this);

char* Platform_getProcessEnv(pid_t pid);

void Platform_getDiskIO(unsigned long int *bytesRead, unsigned long int *bytesWrite, unsigned long int *msTimeSpend);

void Platform_getNetworkIO(unsigned long int *bytesReceived,
                           unsigned long int *packetsReceived,
                           unsigned long int *bytesTransmitted,
                           unsigned long int *packetsTransmitted);

#endif
