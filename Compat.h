#ifndef HEADER_Compat
#define HEADER_Compat
/*
htop - Compat.h
(C) 2020 Christian Göttsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>
#include <sys/stat.h>


int Compat_fstatat(int dirfd,
                   const char* dirpath,
                   const char* pathname,
                   struct stat* statbuf,
                   int flags);

int Compat_readlinkat(int dirfd,
                   const char* dirpath,
                   const char* pathname,
                   char* buf,
                   size_t bufsize);

#endif /* HEADER_Compat */
