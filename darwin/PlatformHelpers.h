#ifndef HEADER_PlatformHelpers
#define HEADER_PlatformHelpers
/*
htop - darwin/PlatformHelpers.h
(C) 2018 Pierre Malhaire, 2020-2021 htop dev team, 2021 Alexander Momchilov
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>


struct kern {
   short int version[3];
};

void Platform_GetKernelVersion(struct kern* k);

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
int Platform_CompareKernelVersion(short int major, short int minor, short int component);

double Platform_calculateNanosecondsPerMachTick(void);

#endif
