/*
htop - Compat.c
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Compat.h"

#include <errno.h>
#include <fcntl.h> // IWYU pragma: keep
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h> // IWYU pragma: keep

#include "XUtils.h" // IWYU pragma: keep


/* GNU/Hurd does not have PATH_MAX in limits.h */
#ifndef PATH_MAX
# define PATH_MAX 4096
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
   if (dirfd != (int)AT_FDCWD || mode != F_OK) {
      errno = EINVAL;
      return -1;
   }

   // Fallback to stat(2)/lstat(2) depending on flags
   struct stat statinfo;
   if (flags) {
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

ssize_t Compat_readlink(openat_arg_t dirfd,
                        const char* pathname,
                        char* buf,
                        size_t bufsize) {

#ifdef HAVE_OPENAT

   char fdPath[32];
   xSnprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", dirfd);

   char dirPath[PATH_MAX + 1];
   ssize_t r = readlink(fdPath, dirPath, sizeof(dirPath) - 1);
   if (r < 0)
      return r;

   dirPath[r] = '\0';

   char linkPath[PATH_MAX + 1];
   xSnprintf(linkPath, sizeof(linkPath), "%s/%s", dirPath, pathname);

#else

   char linkPath[PATH_MAX + 1];
   xSnprintf(linkPath, sizeof(linkPath), "%s/%s", dirfd, pathname);

#endif /* HAVE_OPENAT */

   return readlink(linkPath, buf, bufsize);
}
