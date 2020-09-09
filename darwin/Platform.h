#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - darwin/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "SignalsPanel.h"
#include "CPUMeter.h"
#include "BatteryMeter.h"
#include "DarwinProcess.h"

extern ProcessField Platform_defaultFields[];

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern ProcessFieldData Process_fields[];

extern MeterClass* Platform_meterTypes[];

void Platform_setBindings(Htop_Action* keys);

extern int Platform_numberOfFields;

int Platform_getUptime();

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid();

extern ProcessPidColumn Process_pidColumns[];

double Platform_setCPUValues(Meter* mtr, int cpu);

void Platform_setMemoryValues(Meter* mtr);

void Platform_setSwapValues(Meter* mtr);

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

#endif
