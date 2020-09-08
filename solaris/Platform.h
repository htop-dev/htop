#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - solaris/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include <signal.h>
#include <sys/mkdev.h>
#include <sys/proc.h>
#include <libproc.h>

#define  kill(pid, signal) kill(pid / 1024, signal)

extern ProcessFieldData Process_fields[];
typedef struct var kvar_t;

typedef struct envAccum_ {
   size_t capacity;
   size_t size;
   size_t bytes;
   char *env;
} envAccum;

extern double plat_loadavg[3];

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern ProcessField Platform_defaultFields[];

extern MeterClass* Platform_meterTypes[];

void Platform_setBindings(Htop_Action* keys);

extern int Platform_numberOfFields;

extern char Process_pidFormat[20];

int Platform_getUptime();

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid();

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

#endif
