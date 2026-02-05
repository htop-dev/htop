#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - dragonflybsd/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "Hashtable.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "NetworkIOMeter.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"
#include "CommandLine.h"
#include "generic/gettime.h"
#include "generic/hostname.h"
#include "generic/uname.h"


extern const ScreenDefaults Platform_defaultScreens[];

extern const unsigned int Platform_numberOfDefaultScreens;

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern const MemoryClass Platform_memoryClasses[];

extern const unsigned int Platform_numberOfMemoryClasses;

extern const MeterClass* const Platform_meterTypes[];

bool Platform_init(void);

void Platform_done(void);

void Platform_setBindings(Htop_Action* keys);

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

pid_t Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* this, unsigned int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

void Platform_getFileDescriptors(double* used, double* max);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double* percent, ACPresence* isOnAC);

static inline void Platform_getHostname(char* buffer, size_t size) {
   Generic_hostname(buffer, size);
}

static inline const char* Platform_getRelease(void) {
   return Generic_uname();
}

static inline const char* Platform_getFailedState(void) {
   return NULL;
}

#define PLATFORM_LONG_OPTIONS

static inline void Platform_longOptionsUsage(ATTR_UNUSED const char* name) { }

static inline CommandLineStatus Platform_getLongOption(ATTR_UNUSED int opt, ATTR_UNUSED int argc, ATTR_UNUSED char** argv) {
   return STATUS_ERROR_EXIT;
}

static inline void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec) {
   Generic_gettime_realtime(tv, msec);
}

static inline void Platform_gettime_monotonic(uint64_t* msec) {
   Generic_gettime_monotonic(msec);
}

static inline Hashtable* Platform_dynamicMeters(void) {
   return NULL;
}

static inline void Platform_dynamicMetersDone(ATTR_UNUSED Hashtable* table) { }

static inline void Platform_dynamicMeterInit(ATTR_UNUSED Meter* meter) { }

static inline void Platform_dynamicMeterUpdateValues(ATTR_UNUSED Meter* meter) { }

static inline void Platform_dynamicMeterDisplay(ATTR_UNUSED const Meter* meter, ATTR_UNUSED RichString* out) { }

static inline Hashtable* Platform_dynamicColumns(void) {
   return NULL;
}

static inline void Platform_dynamicColumnsDone(ATTR_UNUSED Hashtable* table) { }

static inline const char* Platform_dynamicColumnName(ATTR_UNUSED unsigned int key) {
   return NULL;
}

static inline bool Platform_dynamicColumnWriteField(ATTR_UNUSED const Process* proc, ATTR_UNUSED RichString* str, ATTR_UNUSED unsigned int key) {
   return false;
}

static inline Hashtable* Platform_dynamicScreens(void) {
   return NULL;
}

static inline void Platform_defaultDynamicScreens(ATTR_UNUSED Settings* settings) { }

static inline void Platform_addDynamicScreen(ATTR_UNUSED ScreenSettings* ss) { }

static inline void Platform_addDynamicScreenAvailableColumns(ATTR_UNUSED Panel* availableColumns, ATTR_UNUSED const char* screen) { }

static inline void Platform_dynamicScreensDone(ATTR_UNUSED Hashtable* screens) { }

#endif
