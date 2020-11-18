/*
htop - Compat.c
(C) 2020 Christian Göttsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Compat.h"

#include <fcntl.h> // IWYU pragma: keep
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h> // IWYU pragma: keep

#include "XUtils.h" // IWYU pragma: keep


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

int Compat_readlinkat(int dirfd,
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
