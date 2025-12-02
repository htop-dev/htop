#ifndef HEADER_Compat
#define HEADER_Compat
/*
htop - linux/Compat.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <fcntl.h>
#include <stddef.h> // IWYU pragma: keep
#include <unistd.h>
#include <sys/stat.h> // IWYU pragma: keep

#include "Macros.h"


int Compat_faccessat(int dirfd,
                     const char* pathname,
                     int mode,
                     int flags);

int Compat_fstatat(int dirfd,
                   const char* dirpath,
                   const char* pathname,
                   struct stat* statbuf,
                   int flags);

#ifdef HAVE_OPENAT

typedef int openat_arg_t;

static inline void Compat_openatArgClose(openat_arg_t dirfd) {
   close(dirfd);
}

static inline int Compat_openat(openat_arg_t dirfd, const char* pathname, int flags) {
   return openat(dirfd, pathname, flags);
}

#else /* HAVE_OPENAT */

typedef const char* openat_arg_t;

static inline void Compat_openatArgClose(openat_arg_t dirpath) {
   (void)dirpath;
}

int Compat_openat(openat_arg_t dirpath, const char* pathname, int flags);

#endif /* HAVE_OPENAT */

ssize_t Compat_readlinkat(int dirfd,
                          const char* dirpath,
                          const char* pathname,
                          char* buf,
                          size_t bufsize);

ssize_t Compat_readlink(openat_arg_t dirfd,
                        const char* pathname,
                        char* buf,
                        size_t bufsize);

ATTR_NONNULL ATTR_ACCESS3_W(2, 3)
ssize_t Compat_readfile(const char* pathname, void* buffer, size_t count);

ATTR_NONNULL ATTR_ACCESS3_W(3, 4)
ssize_t Compat_readfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count);

#endif /* HEADER_Compat */
