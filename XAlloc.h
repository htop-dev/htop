#ifndef HEADER_XAlloc
#define HEADER_XAlloc

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "CRT.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define write_failure(len, ret, filename, lineno)       \
   do {                                                 \
      CRT_done();                                       \
      fprintf(stderr,                                   \
         "Failed to write memory (%zu/%zu) [%s:%d]\n",  \
         (size_t)len,                                   \
         (size_t)ret,                                   \
         filename,                                      \
         lineno);                                       \
      abort();                                          \
   } while(0)

#define oom_failure(filename, lineno)            \
   do {                                          \
      CRT_done();                                \
      fprintf(stderr,                            \
         "Failed to allocate memory [%s:%d]\n",  \
         filename,                               \
         lineno);                                \
      abort();                                   \
   } while(0)

static inline void* xMalloc_impl(size_t size, const char *filename, unsigned int lineno) {
   void *ret = malloc(size);
   if (!ret) {
      oom_failure(filename, lineno);
   }
   return ret;
}
#define xMalloc(size) xMalloc_impl(size, __FILE__, __LINE__)


static inline void* xCalloc_impl(size_t nmemb, size_t size, const char *filename, unsigned int lineno) {
   void *ret = calloc(nmemb, size);
   if (!ret) {
      oom_failure(filename, lineno);
   }
   return ret;
}
#define xCalloc(nmemb, size) xCalloc_impl(nmemb, size, __FILE__, __LINE__)

static inline void* xRealloc_impl(void *ptr, size_t size, const char *filename, unsigned int lineno) {
   void *ret = realloc(ptr, size);
   if (!ret) {
      oom_failure(filename, lineno);
   }
   return ret;
}
#define xRealloc(ptr, size) xRealloc_impl(ptr, size, __FILE__, __LINE__)

static inline char* xStrdup_impl(const char *str,  const char *filename, unsigned int lineno) {
   assert(str);
   char *ret = strdup(str);
   if (!ret) {
      oom_failure(filename, lineno);
   }
   return ret;
}
#define xStrdup(str) xStrdup_impl(str, __FILE__, __LINE__)

#define xAsprintf(strp, fmt, ...)                   \
   do {                                             \
      int ret_ = asprintf(strp, fmt, __VA_ARGS__);  \
      if (ret_ < 0) {                               \
         oom_failure(__FILE__, __LINE__);           \
      }                                             \
   } while(0)

#define xSnprintf(buf, len, ...)                         \
   do {                                                  \
      int len_ = len;                                    \
      int ret_ = snprintf(buf, len_, __VA_ARGS__);       \
      if (ret_ < 0 || ret_ >= len_) {                    \
         write_failure(len_, ret_, __FILE__, __LINE__);  \
      }                                                  \
   } while(0)

#endif /* HEADER_XAlloc */
