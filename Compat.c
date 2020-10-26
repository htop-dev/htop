/*
htop - Compat.c
(C) 2020 Christian Göttsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Compat.h"
#ifndef HAVE_FSTATAT
#include "XUtils.h"
#endif


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
