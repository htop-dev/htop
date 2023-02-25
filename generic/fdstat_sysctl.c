/*
htop - generic/fdstat_sysctl.c
(C) 2022-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "generic/fdstat_sysctl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/types.h> // Shitty FreeBSD upstream headers
#include <sys/sysctl.h>

#include "config.h"


static void Generic_getFileDescriptors_sysctl_internal(
   const char* sysctlname_maxfiles,
   const char* sysctlname_numfiles,
   size_t size_header,
   size_t size_entry,
   double* used,
   double* max
) {
   *used = NAN;
   *max = 65536;

   int max_fd, open_fd;
   size_t len;

   len = sizeof(max_fd);
   if (sysctlname_maxfiles && sysctlbyname(sysctlname_maxfiles, &max_fd, &len, NULL, 0) == 0) {
      if (max_fd) {
         *max = max_fd;
      } else {
         *max = NAN;
      }
   }

   len = sizeof(open_fd);
   if (sysctlname_numfiles && sysctlbyname(sysctlname_numfiles, &open_fd, &len, NULL, 0) == 0) {
      *used = open_fd;
   }

   if (!isnan(*used)) {
      return;
   }

   // If no sysctl arc available, try to guess from the file table size at kern.file
   // The size per entry differs per OS, thus skip if we don't know:
   if (!size_entry)
      return;

   len = 0;
   if (sysctlbyname("kern.file", NULL, &len, NULL, 0) < 0)
      return;

   if (len < size_header)
      return;

   *used = (len - size_header) / size_entry;
}

void Generic_getFileDescriptors_sysctl(double* used, double* max) {
#if defined(HTOP_DARWIN)
   Generic_getFileDescriptors_sysctl_internal(
      "kern.maxfiles", "kern.num_files", 0, 0, used, max);
#elif defined(HTOP_DRAGONFLY)
   Generic_getFileDescriptors_sysctl_internal(
      "kern.maxfiles", NULL, 0, sizeof(struct kinfo_file), used, max);
#elif defined(HTOP_FREEBSD)
   Generic_getFileDescriptors_sysctl_internal(
      "kern.maxfiles", "kern.openfiles", 0, 0, used, max);
#elif defined(HTOP_NETBSD)
   Generic_getFileDescriptors_sysctl_internal(
      "kern.maxfiles", NULL, 0, sizeof(struct kinfo_file), used, max);
#else
#error Unknown platform: Please implement proper way to query open/max file information
#endif
}
