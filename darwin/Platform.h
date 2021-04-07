#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - darwin/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Action.h"
#include "BatteryMeter.h"
#include "CPUMeter.h"
#include "DarwinProcess.h"
#include "DiskIOMeter.h"
#include "NetworkIOMeter.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"
#include "generic/gettime.h"
#include "generic/hostname.h"
#include "generic/uname.h"


extern const ProcessField Platform_defaultFields[];

extern double Platform_timebaseToNS;

extern long Platform_clockTicksPerSec;

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern const MeterClass* const Platform_meterTypes[];

void Platform_init(void);

void Platform_done(void);

void Platform_setBindings(Htop_Action* keys);

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* mtr, unsigned int cpu);

void Platform_setMemoryValues(Meter* mtr);

void Platform_setSwapValues(Meter* mtr);

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

char* Platform_getInodeFilename(pid_t pid, ino_t inode);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double *percent, ACPresence *isOnAC);

static inline void Platform_getHostname(char* buffer, size_t size) {
   Generic_hostname(buffer, size);
}

static inline void Platform_getRelease(char** string) {
   *string = Generic_uname();
}

#define PLATFORM_LONG_OPTIONS

static inline void Platform_longOptionsUsage(ATTR_UNUSED const char* name) { }

static inline bool Platform_getLongOption(ATTR_UNUSED int opt, ATTR_UNUSED int argc, ATTR_UNUSED char** argv) {
   return false;
}

static inline void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec) {
    Generic_gettime_realtime(tv, msec);
}

void Platform_gettime_monotonic(uint64_t* msec);

#endif
