
#include "XAlloc.h"
#include "RichString.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <string.h>

/*{
#include <stdlib.h>
}*/

static char oomMessage[] = "Out of memory!\n";

void* xMalloc(size_t size) {
   void* data = malloc(size);
   if (!data && size > 0) {
      curs_set(1);
      endwin();
      write(2, oomMessage, sizeof oomMessage - 1);
      exit(1);
   }
   return data;
}

void* xCalloc(size_t nmemb, size_t size) {
   void* data = calloc(nmemb, size);
   if (!data) {
      curs_set(1);
      endwin();
      write(2, oomMessage, sizeof oomMessage - 1);
      exit(1);
   }
   return data;
}

void* xRealloc(void* ptr, size_t size) {
   void* data = realloc(ptr, size);
   if (!data && size > 0) {
      curs_set(1);
      endwin();
      write(2, oomMessage, sizeof oomMessage - 1);
      exit(1);
   }
   return data;
}

char* xStrdup(const char* str) {
   char* data = strdup(str);
   if (!data && str) {
      curs_set(1);
      endwin();
      write(2, oomMessage, sizeof oomMessage - 1);
      exit(1);
   }
   return data;
}
