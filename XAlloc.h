#ifndef HEADER_XAlloc
#define HEADER_XAlloc

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <err.h>
#include <assert.h>
#include <stdlib.h>

void fail(void);

void* xMalloc(size_t size);

void* xCalloc(size_t nmemb, size_t size);

void* xRealloc(void* ptr, size_t size);

#undef xAsprintf

#define xAsprintf(strp, fmt, ...) do { int _r=asprintf(strp, fmt, __VA_ARGS__); if (_r < 0) { fail(); } } while(0)

#define xSnprintf(fmt, len, ...) do { int _l=len; int _n=snprintf(fmt, _l, __VA_ARGS__); if (!(_n > -1 && _n < _l)) { curs_set(1); endwin(); err(1, NULL); } } while(0)

#undef xStrdup
#undef xStrdup_
#ifdef NDEBUG
# define xStrdup_ xStrdup
#else
# define xStrdup(str_) (assert(str_), xStrdup_(str_))
#endif

#ifndef __has_attribute // Clang's macro
# define __has_attribute(x) 0
#endif
#if (__has_attribute(nonnull) || \
    ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)))
char* xStrdup_(const char* str) __attribute__((nonnull));
#endif // __has_attribute(nonnull) || GNU C 3.3 or later

char* xStrdup_(const char* str);

#endif
