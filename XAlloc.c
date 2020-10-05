
#include "XAlloc.h"
#include "RichString.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
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

int xAsprintf(char** strp, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int _r = vasprintf(strp, fmt, vl);
   va_end(vl);

   if (_r < 0) {
      fail();
   }

   return _r;
}

int xSnprintf(char* buf, int len, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int _n=vsnprintf(buf, len, fmt, vl);
   va_end(vl);

   if (!(_n > -1 && _n < len)) {
      fail();
   }

   return _n;
}

char* xStrdup(const char* str) {
   char* data = strdup(str);
   if (!data) {
      fail();
   }
   return data;
}
