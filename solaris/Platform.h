#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - solaris/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <libproc.h>
#include <signal.h>
#include <stdbool.h>

#include <sys/mkdev.h>
#include <sys/proc.h>
#include <sys/types.h>

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "NetworkIOMeter.h"
#include "ProcessLocksScreen.h"
#include "SignalsPanel.h"

#include "generic/gettime.h"
#include "generic/hostname.h"
#include "generic/uname.h"


#define  kill(pid, signal) kill(pid / 1024, signal)

typedef struct var kvar_t;

typedef struct envAccum_ {
   size_t capacity;
   size_t size;
   size_t bytes;
   char* env;
} envAccum;

extern double plat_loadavg[3];

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

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

char* Platform_getInodeFilename(pid_t pid, ino_t inode);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double* percent, ACPresence* isOnAC);

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

static inline void Platform_gettime_monotonic(uint64_t* msec) {
    Generic_gettime_monotonic(msec);
}

#endif
