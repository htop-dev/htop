#ifndef HEADER_XUtils
#define HEADER_XUtils
/*
htop - StringUtils.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdio.h>
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

#include "Macros.h"


void fail(void) ATTR_NORETURN;

void* xMalloc(size_t size);

void* xCalloc(size_t nmemb, size_t size);

void* xRealloc(void* ptr, size_t size);

#define String_startsWith(s, match) (strncmp((s),(match),strlen(match)) == 0)
#define String_contains_i(s1, s2) (strcasestr(s1, s2) != NULL)

/*
 * String_startsWith gives better performance if strlen(match) can be computed
 * at compile time (e.g. when they are immutable string literals). :)
 */

char* String_cat(const char* s1, const char* s2);

char* String_trim(const char* in);

int String_eq(const char* s1, const char* s2);

char** String_split(const char* s, char sep, int* n);

void String_freeArray(char** s);

char* String_getToken(const char* line, const unsigned short int numMatch);

char* String_readLine(FILE* fd);

ATTR_FORMAT(printf, 2, 3)
int xAsprintf(char **strp, const char* fmt, ...);

ATTR_FORMAT(printf, 3, 4)
int xSnprintf(char *buf, int len, const char* fmt, ...);

char* xStrdup(const char* str) ATTR_NONNULL;

#endif
