/*
htop - darwin/PlatformHelpers.c
(C) 2018 Pierre Malhaire, 2020-2021 htop dev team, 2021 Alexander Momchilov
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/PlatformHelpers.h"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

#include "CRT.h"

#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#endif


void Platform_GetKernelVersion(struct kern* k) {
   static short int version_[3] = {0};
   if (!version_[0]) {
      // just in case it fails someday
      version_[0] = version_[1] = version_[2] = -1;
      char str[256] = {0};
      size_t size = sizeof(str);
      int ret = sysctlbyname("kern.osrelease", str, &size, NULL, 0);
      if (ret == 0) {
         sscanf(str, "%hd.%hd.%hd", &version_[0], &version_[1], &version_[2]);
      }
   }
   memcpy(k->version, version_, sizeof(version_));
}

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
int Platform_CompareKernelVersion(short int major, short int minor, short int component) {
   struct kern k;
   Platform_GetKernelVersion(&k);

   if (k.version[0] != major) {
      return k.version[0] - major;
   }
   if (k.version[1] != minor) {
      return k.version[1] - minor;
   }
   if (k.version[2] != component) {
      return k.version[2] - component;
   }

   return 0;
}

double Platform_calculateNanosecondsPerMachTick() {
   // Check if we can determine the timebase used on this system.
   // If the API is unavailable assume we get our timebase in nanoseconds.
#ifdef HAVE_MACH_TIMEBASE_INFO
   mach_timebase_info_data_t info;
   mach_timebase_info(&info);
   return (double)info.numer / (double)info.denom;
#else
   return 1.0;
#endif
}
