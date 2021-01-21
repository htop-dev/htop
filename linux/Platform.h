#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - linux/Platform.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "Meter.h"
#include "NetworkIOMeter.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"
#include "generic/gettime.h"
#include "generic/hostname.h"
#include "generic/uname.h"

/* GNU/Hurd does not have PATH_MAX in limits.h */
#ifndef PATH_MAX
   #define PATH_MAX 4096
#endif


extern const ProcessField Platform_defaultFields[];

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

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

void Platform_setZramValues(Meter* this);

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

char* Platform_getInodeFilename(pid_t pid, ino_t inode);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

void Platform_getPressureStall(const char *file, bool some, double* ten, double* sixty, double* threehundred);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double *percent, ACPresence *isOnAC);

static inline void Platform_getHostname(char* buffer, size_t size) {
   Generic_hostname(buffer, size);
}

static inline void Platform_getRelease(char** string) {
   *string = Generic_uname();
}

#ifdef HAVE_LIBCAP
   #define PLATFORM_LONG_OPTIONS \
      {"drop-capabilities", optional_argument, 0, 160},
#else
   #define PLATFORM_LONG_OPTIONS
#endif

void Platform_longOptionsUsage(const char* name);

bool Platform_getLongOption(int opt, int argc, char** argv);

static inline void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec) {
    Generic_gettime_realtime(tv, msec);
}

static inline void Platform_gettime_monotonic(uint64_t* msec) {
    Generic_gettime_monotonic(msec);
}

#endif
