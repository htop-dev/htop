#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - pcp/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pcp/pmapi.h>
#include <sys/time.h>
#include <sys/types.h>

/* use htop config.h values for these macros, not pcp values */
#undef PACKAGE_URL
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT

#include "Action.h"
#include "BatteryMeter.h"
#include "DiskIOMeter.h"
#include "Hashtable.h"
#include "Meter.h"
#include "NetworkIOMeter.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "RichString.h"
#include "SignalsPanel.h"
#include "CommandLine.h"

#include "pcp/PCPDynamicColumn.h"
#include "pcp/PCPDynamicMeter.h"
#include "pcp/PCPDynamicScreen.h"
#include "pcp/PCPMetric.h"


typedef struct Platform_ {
   int context;               /* PMAPI(3) context identifier */
   size_t totalMetrics;       /* total number of all metrics */
   const char** names;        /* name array indexed by Metric */
   pmID* pmids;               /* all known metric identifiers */
   pmID* fetch;               /* enabled identifiers for sampling */
   pmDesc* descs;             /* metric desc array indexed by Metric */
   pmResult* result;          /* sample values result indexed by Metric */
   PCPDynamicMeters meters;   /* dynamic meters via configuration files */
   PCPDynamicColumns columns; /* dynamic columns via configuration files */
   PCPDynamicScreens screens; /* dynamic screens via configuration files */
   struct timeval offset;     /* time offset used in archive mode only */
   long long btime;           /* boottime in seconds since the epoch */
   char* release;             /* uname and distro from this context */
   int pidmax;                /* maximum platform process identifier */
   unsigned int ncpu;         /* maximum processor count configured */
} Platform;

extern const ScreenDefaults Platform_defaultScreens[];

extern const unsigned int Platform_numberOfDefaultScreens;

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern const MeterClass* const Platform_meterTypes[];

bool Platform_init(void);

void Platform_done(void);

void Platform_setBindings(Htop_Action* keys);

int Platform_getUptime(void);

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

long long Platform_getBootTime(void);

unsigned int Platform_getMaxCPU(void);

int Platform_getMaxPid(void);

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

void Platform_setZramValues(Meter* this);

void Platform_setZfsArcValues(Meter* this);

void Platform_setZfsCompressedArcValues(Meter* this);

char* Platform_getProcessEnv(pid_t pid);

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid);

void Platform_getPressureStall(const char* file, bool some, double* ten, double* sixty, double* threehundred);

bool Platform_getDiskIO(DiskIOData* data);

bool Platform_getNetworkIO(NetworkIOData* data);

void Platform_getBattery(double* percent, ACPresence* isOnAC);

void Platform_getHostname(char* buffer, size_t size);

void Platform_getRelease(char** string);

enum {
   PLATFORM_LONGOPT_HOST = 128,
   PLATFORM_LONGOPT_TIMEZONE,
   PLATFORM_LONGOPT_HOSTZONE,
};

#define PLATFORM_LONG_OPTIONS \
      {PMLONGOPT_HOST, optional_argument, 0, PLATFORM_LONGOPT_HOST}, \
      {PMLONGOPT_TIMEZONE, optional_argument, 0, PLATFORM_LONGOPT_TIMEZONE}, \
      {PMLONGOPT_HOSTZONE, optional_argument, 0, PLATFORM_LONGOPT_HOSTZONE}, \

void Platform_longOptionsUsage(const char* name);

CommandLineStatus Platform_getLongOption(int opt, int argc, char** argv);

extern pmOptions opts;

size_t Platform_addMetric(PCPMetric id, const char* name);

void Platform_getFileDescriptors(double* used, double* max);

void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec);

void Platform_gettime_monotonic(uint64_t* msec);

Hashtable* Platform_dynamicMeters(void);

void Platform_dynamicMetersDone(Hashtable* meters);

void Platform_dynamicMeterInit(Meter* meter);

void Platform_dynamicMeterUpdateValues(Meter* meter);

void Platform_dynamicMeterDisplay(const Meter* meter, RichString* out);

Hashtable* Platform_dynamicColumns(void);

void Platform_dynamicColumnsDone(Hashtable* columns);

const char* Platform_dynamicColumnName(unsigned int key);

bool Platform_dynamicColumnWriteField(const Process* proc, RichString* str, unsigned int key);

Hashtable* Platform_dynamicScreens(void);

void Platform_defaultDynamicScreens(Settings* settings);

void Platform_addDynamicScreen(ScreenSettings* ss);

void Platform_addDynamicScreenAvailableColumns(Panel* availableColumns, const char* screen);

void Platform_dynamicScreensDone(Hashtable* screens);

void Platform_updateTables(Machine* host);

#endif
