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
#include "DiskIOMeter.h"
#include "NetworkIOMeter.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"
#include "generic/gettime.h"
#include "unsupported/UnsupportedProcess.h"


extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern const ProcessField Platform_defaultFields[];

extern const MeterClass* const Platform_meterTypes[];

void Platform_init(void);

void Platform_done(void);

void Platform_setBindings(Htop_Action* keys);

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* this, unsigned int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

bool Process_isThread(const Process* this);

char* Platform_getProcessEnv(pid_t pid);

char* Platform_getInodeFilename(pid_t pid, ino_t inode);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double *percent, ACPresence *isOnAC);

void Platform_getHostname(char* buffer, size_t size);

void Platform_getRelease(char** string);

#define PLATFORM_LONG_OPTIONS

static inline void Platform_longOptionsUsage(ATTR_UNUSED const char* name) { }

static inline bool Platform_getLongOption(ATTR_UNUSED int opt, ATTR_UNUSED int argc, ATTR_UNUSED char** argv) {
   return false;
}

static inline void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec) {
    Generic_gettime_realtime(tv, msec);
}

static inline void Platform_gettime_monotonic(uint64_t* msec) {
    Generic_gettime_monotonic(msec);
}

#endif
