#ifndef HEADER_Compat
#define HEADER_Compat
/*
htop - Compat.h
(C) 2020 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <assert.h> // IWYU pragma: keep
#include <fcntl.h>
#include <stddef.h> // IWYU pragma: keep
#include <unistd.h>
#include <sys/stat.h> // IWYU pragma: keep


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

/*
 * static_assert() hack for pre-C11
 * TODO: drop after moving to -std=c11 or newer
 */

/* C11 guarantees _Static_assert is a keyword */
#if (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 201112L
# if !defined(_Static_assert)
#  define _Static_assert(expr, msg)                                    \
   extern int (*__Static_assert_function (void))                       \
      [!!sizeof (struct { int __error_if_negative: (expr) ? 2 : -1; })]
# endif
#endif

/* C23 will guarantee static_assert is a keyword or a macro */
/* FIXME: replace 202300L with proper value once C23 is published */
#if (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 202300L
# if !defined(static_assert)
#  define static_assert(expr, msg) _Static_assert(expr, msg)
# endif
#endif

#endif /* HEADER_Compat */
