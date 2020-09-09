#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - dragonflybsd/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

extern ProcessField Platform_defaultFields[];

extern int Platform_numberOfFields;

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

void Platform_setBindings(Htop_Action* keys);

extern MeterClass* Platform_meterTypes[];

int Platform_getUptime();

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid();

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

void Platform_setTasksValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

#endif
