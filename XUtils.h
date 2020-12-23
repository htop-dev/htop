#ifndef HEADER_XUtils
#define HEADER_XUtils
/*
htop - StringUtils.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep
#include <sys/types.h>

#include "Compat.h"
#include "Macros.h"


void fail(void) ATTR_NORETURN;

void* xMalloc(size_t size);

void* xMallocArray(size_t nmemb, size_t size);

void* xCalloc(size_t nmemb, size_t size);

void* xRealloc(void* ptr, size_t size);

void* xReallocArray(void* ptr, size_t nmemb, size_t size);

/*
 * String_startsWith gives better performance if strlen(match) can be computed
 * at compile time (e.g. when they are immutable string literals). :)
 */
static inline bool String_startsWith(const char* s, const char* match) {
   return strncmp(s, match, strlen(match)) == 0;
}

static inline bool String_contains_i(const char* s1, const char* s2) {
   return strcasestr(s1, s2) != NULL;
}

static inline bool String_eq(const char* s1, const char* s2) {
   return strcmp(s1, s2) == 0;
}

char* String_cat(const char* s1, const char* s2);

char* String_trim(const char* in);

char** String_split(const char* s, char sep, size_t* n);

void String_freeArray(char** s);

char* String_getToken(const char* line, unsigned short int numMatch);

char* String_readLine(FILE* fd);

ATTR_FORMAT(printf, 2, 3)
int xAsprintf(char** strp, const char* fmt, ...);

ATTR_FORMAT(printf, 3, 4)
int xSnprintf(char* buf, size_t len, const char* fmt, ...);

char* xStrdup(const char* str) ATTR_NONNULL;

char* xStrndup(const char* str, size_t len) ATTR_NONNULL;

ssize_t xReadfile(const char* pathname, void* buffer, size_t count);
ssize_t xReadfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count);

#endif
