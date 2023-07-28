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


void Platform_GetKernelVersion(KernelVersion* k) {
   static KernelVersion cachedKernelVersion;

   if (!cachedKernelVersion.major) {
      // just in case it fails someday
      cachedKernelVersion = (KernelVersion) { -1, -1, -1 };
      char str[256] = {0};
      size_t size = sizeof(str);
      int ret = sysctlbyname("kern.osrelease", str, &size, NULL, 0);
      if (ret == 0) {
         sscanf(str, "%hd.%hd.%hd", &cachedKernelVersion.major, &cachedKernelVersion.minor, &cachedKernelVersion.patch);
      }
   }
   memcpy(k, &cachedKernelVersion, sizeof(cachedKernelVersion));
}

int Platform_CompareKernelVersion(KernelVersion v) {
   struct KernelVersion actualVersion;
   Platform_GetKernelVersion(&actualVersion);

   if (actualVersion.major != v.major) {
      return actualVersion.major - v.major;
   }
   if (actualVersion.minor != v.minor) {
      return actualVersion.minor - v.minor;
   }
   if (actualVersion.patch != v.patch) {
      return actualVersion.patch - v.patch;
   }

   return 0;
}

bool Platform_KernelVersionIsBetween(KernelVersion lowerBound, KernelVersion upperBound) {
   return 0 <= Platform_CompareKernelVersion(lowerBound)
      && Platform_CompareKernelVersion(upperBound) < 0;
}

void Platform_getCPUBrandString(char* cpuBrandString, size_t cpuBrandStringSize) {
   if (sysctlbyname("machdep.cpu.brand_string", cpuBrandString, &cpuBrandStringSize, NULL, 0) == -1) {
      fprintf(stderr,
         "WARN: Unable to determine the CPU brand string.\n"
         "errno: %i, %s\n", errno, strerror(errno));

      String_safeStrncpy(cpuBrandString, "UNKNOWN!", cpuBrandStringSize);
   }
}

// Adapted from https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment
bool Platform_isRunningTranslated(void) {
   int ret = 0;
   size_t size = sizeof(ret);
   errno = 0;
   if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
      if (errno == ENOENT)
         return false;

      fprintf(stderr,
         "WARN: Could not determine if this process was running in a translation environment like Rosetta 2.\n"
         "Assuming that we're not.\n"
         "errno: %i, %s\n", errno, strerror(errno));

      return false;
   }
   return ret;
}

double Platform_calculateNanosecondsPerMachTick(void) {
   // Check if we can determine the timebase used on this system.
   // If the API is unavailable assume we get our timebase in nanoseconds.
#ifndef HAVE_MACH_TIMEBASE_INFO
   return 1.0;
#else
   mach_timebase_info_data_t info;

   /* WORKAROUND for `mach_timebase_info` giving incorrect values on M1 under Rosetta 2.
    *    rdar://FB9546856 https://openradar.appspot.com/radar?id=5055988478509056
    *
    *    We don't know exactly what feature/attribute of the M1 chip causes this mistake under Rosetta 2.
    *    Until we have more Apple ARM chips to compare against, the best we can do is special-case
    *    the "Apple M1" chip specifically when running under Rosetta 2.
    */

   char cpuBrandString[1024] = "";
   Platform_getCPUBrandString(cpuBrandString, sizeof(cpuBrandString));

   bool isRunningUnderRosetta2 = Platform_isRunningTranslated();

   // Kernel version 20.0.0 is macOS 11.0 (Big Sur)
   bool isBuggedVersion = Platform_KernelVersionIsBetween((KernelVersion) {20, 0, 0}, (KernelVersion) {999, 999, 999});

   if (isRunningUnderRosetta2 && String_eq(cpuBrandString, "Apple M1") && isBuggedVersion) {
      // In this case `mach_timebase_info` provides the wrong value, so we hard-code the correct factor,
      // as determined from `mach_timebase_info` when the process running natively.
      info = (mach_timebase_info_data_t) { .numer = 125, .denom = 3 };
   } else {
      // No workarounds needed, use the OS-provided value.
      mach_timebase_info(&info);
   }

   return (double)info.numer / (double)info.denom;
#endif
}
