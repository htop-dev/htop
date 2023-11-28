#ifndef HEADER_XUtils
#define HEADER_XUtils
/*
htop - StringUtils.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

// IWYU pragma: no_include "config.h"
#ifndef PACKAGE
// strchrnul() needs _GNU_SOURCE defined, see PR #1337 for details
#error "Must have #include \"config.h\" line at the top of the file that includes these XUtils helper functions"
#endif

#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

#include "Compat.h"
#include "Macros.h"


void fail(void) ATTR_NORETURN;

void* xMalloc(size_t size) ATTR_ALLOC_SIZE1(1) ATTR_MALLOC;

void* xMallocArray(size_t nmemb, size_t size) ATTR_ALLOC_SIZE2(1, 2) ATTR_MALLOC;

void* xCalloc(size_t nmemb, size_t size) ATTR_ALLOC_SIZE2(1, 2) ATTR_MALLOC;

void* xRealloc(void* ptr, size_t size) ATTR_ALLOC_SIZE1(2);

void* xReallocArray(void* ptr, size_t nmemb, size_t size) ATTR_ALLOC_SIZE2(2, 3);

void* xReallocArrayZero(void* ptr, size_t prevmemb, size_t newmemb, size_t size) ATTR_ALLOC_SIZE2(3, 4);

/*
 * String_startsWith gives better performance if strlen(match) can be computed
 * at compile time (e.g. when they are immutable string literals). :)
 */
ATTR_NONNULL
static inline bool String_startsWith(const char* s, const char* match) {
   return strncmp(s, match, strlen(match)) == 0;
}

bool String_contains_i(const char* s1, const char* s2, bool multi);

ATTR_NONNULL
static inline bool String_eq(const char* s1, const char* s2) {
   return strcmp(s1, s2) == 0;
}

ATTR_NONNULL
char* String_cat(const char* s1, const char* s2) ATTR_MALLOC;

ATTR_NONNULL
char* String_trim(const char* in) ATTR_MALLOC;

ATTR_NONNULL_N(1)
char** String_split(const char* s, char sep, size_t* n);

void String_freeArray(char** s);

ATTR_NONNULL
char* String_readLine(FILE* fd) ATTR_MALLOC;

ATTR_NONNULL
static inline char* String_strchrnul(const char* s, int c) {
#ifdef HAVE_STRCHRNUL
   return strchrnul(s, c);
#else
   char* result = strchr(s, c);
   if (result)
      return result;
   return strchr(s, '\0');
#endif
}

/* Always null-terminates dest. Caller must pass a strictly positive size. */
ATTR_ACCESS3_W(1, 3)
ATTR_ACCESS3_R(2, 3)
size_t String_safeStrncpy(char* restrict dest, const char* restrict src, size_t size);

ATTR_FORMAT(printf, 2, 3)
ATTR_NONNULL_N(1, 2)
int xAsprintf(char** strp, const char* fmt, ...);

ATTR_FORMAT(printf, 3, 4)
ATTR_ACCESS3_W(1, 2)
int xSnprintf(char* buf, size_t len, const char* fmt, ...);

char* xStrdup(const char* str) ATTR_NONNULL ATTR_MALLOC;
void free_and_xStrdup(char** ptr, const char* str);

ATTR_ACCESS3_R(1, 2)
char* xStrndup(const char* str, size_t len) ATTR_NONNULL ATTR_MALLOC;

ATTR_ACCESS3_W(2, 3)
ssize_t xReadfile(const char* pathname, void* buffer, size_t count);
ATTR_ACCESS3_W(3, 4)
ssize_t xReadfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count);

ATTR_ACCESS3_R(2, 3)
ssize_t full_write(int fd, const void* buf, size_t count);

/* Compares floating point values for ordering data entries. In this function,
   NaN is considered "less than" any other floating point value (regardless of
   sign), and two NaNs are considered "equal" regardless of payload. */
int compareRealNumbers(double a, double b);

/* Computes the sum of all positive floating point values in an array.
   NaN values in the array are skipped. The returned sum will always be
   nonnegative. */
double sumPositiveValues(const double* array, size_t count);

/* IEC unit prefixes */
static const char unitPrefixes[] = { 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y', 'R', 'Q' };

#endif
