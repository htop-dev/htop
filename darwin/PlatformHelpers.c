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
