#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - linux/Platform.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "MainPanel.h"
#include "BatteryMeter.h"
#include "LinuxProcess.h"
#include "SignalsPanel.h"

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

void Platform_getPressureStall(const char *file, bool some, double* ten, double* sixty, double* threehundred);

void Platform_getDiskIO(unsigned long int *bytesRead, unsigned long int *bytesWrite, unsigned long int *msTimeSpend);

#endif
