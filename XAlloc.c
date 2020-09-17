
#include "XAlloc.h"
#include "RichString.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>


void fail() {
   curs_set(1);
   endwin();
   err(1, NULL);
}

void* xMalloc(size_t size) {
   void* data = malloc(size);
   if (!data && size > 0) {
      fail();
   }
   return data;
}

void* xCalloc(size_t nmemb, size_t size) {
   void* data = calloc(nmemb, size);
   if (!data && nmemb > 0 && size > 0) {
      fail();
   }
   return data;
}

void* xRealloc(void* ptr, size_t size) {
   void* data = realloc(ptr, size);
   if (!data && size > 0) {
      fail();
   }
   return data;
}

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

char* xStrdup_(const char* str) {
   char* data = strdup(str);
   if (!data) {
      fail();
   }
   return data;
}
