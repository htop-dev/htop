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

#include <dirent.h>
#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

#include "Compat.h"
#include "Macros.h"


ATTR_NORETURN
void fail(void);

ATTR_RETNONNULL ATTR_MALLOC ATTR_ALLOC_SIZE1(1)
void* xMalloc(size_t size);

ATTR_RETNONNULL ATTR_MALLOC ATTR_ALLOC_SIZE2(1, 2)
void* xMallocArray(size_t nmemb, size_t size);

ATTR_RETNONNULL ATTR_MALLOC ATTR_ALLOC_SIZE2(1, 2)
void* xCalloc(size_t nmemb, size_t size);

ATTR_RETNONNULL ATTR_ALLOC_SIZE1(2)
void* xRealloc(void* ptr, size_t size);

ATTR_RETNONNULL ATTR_ALLOC_SIZE2(2, 3)
void* xReallocArray(void* ptr, size_t nmemb, size_t size);

ATTR_RETNONNULL ATTR_ALLOC_SIZE2(3, 4)
void* xReallocArrayZero(void* ptr, size_t prevmemb, size_t newmemb, size_t size);

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

static inline bool String_eq_nullable(const char* s1, const char* s2) {
   if (s1 == s2)
      return true;

   if (s1 && s2)
      return String_eq(s1, s2);

   return false;
}

ATTR_NONNULL ATTR_RETNONNULL ATTR_MALLOC
char* String_cat(const char* s1, const char* s2);

ATTR_NONNULL ATTR_RETNONNULL ATTR_MALLOC
char* String_trim(const char* in);

ATTR_NONNULL_N(1) ATTR_RETNONNULL
char** String_split(const char* s, char sep, size_t* n);

void String_freeArray(char** s);

ATTR_NONNULL ATTR_MALLOC
char* String_readLine(FILE* fp);

ATTR_NONNULL ATTR_RETNONNULL
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
ATTR_NONNULL ATTR_ACCESS3_W(1, 3) ATTR_ACCESS3_R(2, 3)
size_t String_safeStrncpy(char* restrict dest, const char* restrict src, size_t size);

ATTR_FORMAT(printf, 2, 3) ATTR_NONNULL_N(1, 2)
int xAsprintf(char** strp, const char* fmt, ...);

ATTR_FORMAT(printf, 3, 4) ATTR_NONNULL_N(1, 3) ATTR_ACCESS3_W(1, 2)
int xSnprintf(char* buf, size_t len, const char* fmt, ...);

ATTR_NONNULL ATTR_RETNONNULL ATTR_MALLOC
char* xStrdup(const char* str);

ATTR_NONNULL
void free_and_xStrdup(char** ptr, const char* str);

ATTR_NONNULL ATTR_RETNONNULL ATTR_MALLOC ATTR_ACCESS3_R(1, 2)
char* xStrndup(const char* str, size_t len);

ATTR_NONNULL ATTR_ACCESS3_W(2, 3)
ssize_t xReadfile(const char* pathname, void* buffer, size_t count);
ATTR_NONNULL ATTR_ACCESS3_W(3, 4)
ssize_t xReadfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count);

ATTR_NONNULL ATTR_ACCESS3_R(2, 3)
ssize_t full_write(int fd, const void* buf, size_t count);

ATTR_NONNULL
static inline ssize_t full_write_str(int fd, const char* str) {
   return full_write(fd, str, strlen(str));
}

/* Compares floating point values for ordering data entries. In this function,
   NaN is considered "less than" any other floating point value (regardless of
   sign), and two NaNs are considered "equal" regardless of payload. */
int compareRealNumbers(double a, double b);

/* Computes the sum of all positive floating point values in an array.
   NaN values in the array are skipped. The returned sum will always be
   nonnegative. */
ATTR_NONNULL ATTR_ACCESS3_R(1, 2)
double sumPositiveValues(const double* array, size_t count);

/* Counts the number of digits needed to print "n" with a given base.
   If "n" is zero, returns 1. This function expects small numbers to
   appear often, hence it uses a O(log(n)) time algorithm. */
size_t countDigits(size_t n, size_t base);

/* Returns the number of trailing zero bits */
#if defined(HAVE_BUILTIN_CTZ)
static inline unsigned int countTrailingZeros(unsigned int x) {
   return !x ? 32 : __builtin_ctz(x);
}
#else
unsigned int countTrailingZeros(unsigned int x);
#endif

/* IEC unit prefixes */
static const char unitPrefixes[] = { 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y', 'R', 'Q' };

static inline bool skipEndOfLine(FILE* fp) {
   char buffer[1024];
   while (fgets(buffer, sizeof(buffer), fp)) {
      if (strchr(buffer, '\n')) {
         return true;
      }
   }
   return false;
}

static inline int xDirfd(DIR* dirp) {
   int r = dirfd(dirp);
   assert(r >= 0);
   return r;
}

#endif
