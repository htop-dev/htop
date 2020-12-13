/*
htop - Compat.c
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Compat.h"

#include <errno.h>
#include <fcntl.h> // IWYU pragma: keep
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h> // IWYU pragma: keep

#include "XUtils.h" // IWYU pragma: keep

#ifdef HAVE_HOST_GET_CLOCK_SERVICE
#include <mach/clock.h>
#include <mach/mach.h>
#endif


int Compat_faccessat(int dirfd,
                     const char* pathname,
                     int mode,
                     int flags) {
   int ret;

#ifdef HAVE_FACCESSAT

   // Implementation note: AT_SYMLINK_NOFOLLOW unsupported on FreeBSD, fallback to lstat in that case

   errno = 0;

   ret = faccessat(dirfd, pathname, mode, flags);
   if (!ret || errno != EINVAL)
      return ret;

#endif

   // Error out on unsupported configurations
   if (dirfd != AT_FDCWD || mode != F_OK) {
      errno = EINVAL;
      return -1;
   }

   // Fallback to stat(2)/lstat(2) depending on flags
   struct stat statinfo;
   if(flags) {
      ret = lstat(pathname, &statinfo);
   } else {
      ret = stat(pathname, &statinfo);
   }

   return ret;
}

int Compat_fstatat(int dirfd,
                   const char* dirpath,
                   const char* pathname,
                   struct stat* statbuf,
                   int flags) {

#ifdef HAVE_FSTATAT

   (void)dirpath;

   return fstatat(dirfd, pathname, statbuf, flags);

#else

   (void)dirfd;

   char path[4096];
   xSnprintf(path, sizeof(path), "%s/%s", dirpath, pathname);

   if (flags & AT_SYMLINK_NOFOLLOW)
      return lstat(path, statbuf);

   return stat(path, statbuf);

#endif
}

#ifndef HAVE_OPENAT

int Compat_openat(const char* dirpath,
                  const char* pathname,
                  int flags) {

   char path[4096];
   xSnprintf(path, sizeof(path), "%s/%s", dirpath, pathname);

   return open(path, flags);
}

#endif /* !HAVE_OPENAT */

ssize_t Compat_readlinkat(int dirfd,
                          const char* dirpath,
                          const char* pathname,
                          char* buf,
                          size_t bufsize) {

#ifdef HAVE_READLINKAT

   (void)dirpath;

   return readlinkat(dirfd, pathname, buf, bufsize);

#else

   (void)dirfd;

   char path[4096];
   xSnprintf(path, sizeof(path), "%s/%s", dirpath, pathname);

   return readlink(path, buf, bufsize);

#endif
}

int Compat_clock_monotonic_gettime(struct timespec *tp) {

#if defined(HAVE_CLOCK_GETTIME)

   return clock_gettime(CLOCK_MONOTONIC, tp);

#elif defined(HAVE_HOST_GET_CLOCK_SERVICE)

   clock_serv_t cclock;
   mach_timespec_t mts;

   host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);

   tp->tv_sec = mts.tv_sec;
   tp->tv_nsec = mts.tv_nsec;

   return 0;

#else

#error No Compat_clock_monotonic_gettime() implementation!

#endif

}
