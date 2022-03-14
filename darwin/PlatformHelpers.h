#ifndef HEADER_PlatformHelpers
#define HEADER_PlatformHelpers
/*
htop - darwin/PlatformHelpers.h
(C) 2018 Pierre Malhaire, 2020-2022 htop dev team, 2021 Alexander Momchilov
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>


typedef struct KernelVersion {
   short int major;
   short int minor;
   short int patch;
} KernelVersion;

void Platform_GetKernelVersion(KernelVersion* k);

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
int Platform_CompareKernelVersion(KernelVersion v);

// lowerBound <= currentVersion < upperBound
bool Platform_KernelVersionIsBetween(KernelVersion lowerBound, KernelVersion upperBound);

double Platform_calculateNanosecondsPerMachTick(void);

void Platform_getCPUBrandString(char *cpuBrandString, size_t cpuBrandStringSize);

bool Platform_isRunningTranslated(void);

double Platform_calculateNanosecondsPerMachTick(void);

#endif
