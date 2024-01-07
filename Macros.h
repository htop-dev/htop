#ifndef HEADER_Macros
#define HEADER_Macros
/*
htop - Macros.h
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <assert.h> // IWYU pragma: keep
#include <math.h>
#include <stdbool.h>
#include <string.h> // IWYU pragma: keep


#ifndef MINIMUM
#define MINIMUM(a, b)                  ((a) < (b) ? (a) : (b))
#endif

#ifndef MAXIMUM
#define MAXIMUM(a, b)                  ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)            (assert((low) <= (high)), ((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x)                   (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef SPACESHIP_NUMBER
#define SPACESHIP_NUMBER(a, b)         (((a) > (b)) - ((a) < (b)))
#endif

#ifndef SPACESHIP_NULLSTR
#define SPACESHIP_NULLSTR(a, b)        strcmp((a) ? (a) : "", (b) ? (b) : "")
#endif

#ifndef SPACESHIP_DEFAULTSTR
#define SPACESHIP_DEFAULTSTR(a, b, s)  strcmp((a) ? (a) : (s), (b) ? (b) : (s))
#endif

#ifdef  __GNUC__  // defined by GCC and Clang

#define ATTR_FORMAT(type, index, check) __attribute__((format (type, index, check)))
#define ATTR_NORETURN                   __attribute__((noreturn))
#define ATTR_UNUSED                     __attribute__((unused))
#define ATTR_MALLOC                     __attribute__((malloc))

#else /* __GNUC__ */

#define ATTR_FORMAT(type, index, check)
#define ATTR_NORETURN
#define ATTR_UNUSED
#define ATTR_MALLOC

#endif /* __GNUC__ */

#ifdef HAVE_ATTR_NONNULL

#define ATTR_NONNULL                    __attribute__((nonnull))
#define ATTR_NONNULL_N(...)             __attribute__((nonnull(__VA_ARGS__)))

#else

#define ATTR_NONNULL
#define ATTR_NONNULL_N(...)

#endif /* HAVE_ATTR_NONNULL */

#ifdef HAVE_ATTR_ALLOC_SIZE

#define ATTR_ALLOC_SIZE1(a)             __attribute__((alloc_size (a)))
#define ATTR_ALLOC_SIZE2(a, b)          __attribute__((alloc_size (a, b)))

#else

#define ATTR_ALLOC_SIZE1(a)
#define ATTR_ALLOC_SIZE2(a, b)

#endif /* HAVE_ATTR_ALLOC_SIZE */

#ifdef HAVE_ATTR_ACCESS

#define ATTR_ACCESS2(mode, ref)         __attribute__((access (mode, ref)))
#define ATTR_ACCESS3(mode, ref, size)   __attribute__((access (mode, ref, size)))

#else

#define ATTR_ACCESS2(mode, ref)
#define ATTR_ACCESS3(mode, ref, size)

#endif /* HAVE_ATTR_ACCESS */

#define ATTR_ACCESS2_R(ref)              ATTR_ACCESS2(read_only, ref)
#define ATTR_ACCESS3_R(ref, size)        ATTR_ACCESS3(read_only, ref, size)

#define ATTR_ACCESS2_RW(ref)             ATTR_ACCESS2(read_write, ref)
#define ATTR_ACCESS3_RW(ref, size)       ATTR_ACCESS3(read_write, ref, size)

#define ATTR_ACCESS2_W(ref)              ATTR_ACCESS2(write_only, ref)
#define ATTR_ACCESS3_W(ref, size)        ATTR_ACCESS3(write_only, ref, size)

// ignore casts discarding const specifier, e.g.
//     const char []     ->  char * / void *
//     const char *[2]'  ->  char *const *
#if defined(__clang__)
#define IGNORE_WCASTQUAL_BEGIN  _Pragma("clang diagnostic push") \
                                _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#define IGNORE_WCASTQUAL_END    _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define IGNORE_WCASTQUAL_BEGIN  _Pragma("GCC diagnostic push") \
                                _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define IGNORE_WCASTQUAL_END    _Pragma("GCC diagnostic pop")
#else
#define IGNORE_WCASTQUAL_BEGIN
#define IGNORE_WCASTQUAL_END
#endif

/* Cheaper function for checking NaNs. Unlike the standard isnan(), this may
   throw an FP exception on a "signaling NaN".
   (ISO/IEC TS 18661-1 and the C23 standard stated that isnan() throws no
   exceptions even with a "signaling NaN") */
static inline bool isNaN(double x) {
   return !isgreaterequal(x, x);
}

/* Checks if x >= 0.0 but returns false if x is NaN. Because IEEE 754 considers
   -0.0 == 0.0, this function treats both zeros as nonnegative. */
static inline bool isNonnegative(double x) {
   return isgreaterequal(x, 0.0);
}

/* Checks if x > 0.0 but returns false if x is NaN. */
static inline bool isPositive(double x) {
   return isgreater(x, 0.0);
}

/* This subtraction is used by Linux / NetBSD / OpenBSD for calculation of CPU usage items. */
static inline unsigned long long saturatingSub(unsigned long long a, unsigned long long b) {
   return a > b ? a - b : 0;
}

#endif
