/*
htop - StringUtils.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "XUtils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CRT.h"
#include "Macros.h"


void fail(void) {
   CRT_done();
   abort();

   _exit(1); // Should never reach here
}

void* xMalloc(size_t size) {
   assert(size > 0);
   void* data = malloc(size);
   if (!data) {
      fail();
   }
   return data;
}

void* xMallocArray(size_t nmemb, size_t size) {
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   return xMalloc(nmemb * size);
}

void* xCalloc(size_t nmemb, size_t size) {
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   void* data = calloc(nmemb, size);
   if (!data) {
      fail();
   }
   return data;
}

void* xRealloc(void* ptr, size_t size) {
   assert(size > 0);
   void* data = realloc(ptr, size); // deepcode ignore MemoryLeakOnRealloc: this goes to fail()
   if (!data) {
      free(ptr);
      fail();
   }
   return data;
}

void* xReallocArray(void* ptr, size_t nmemb, size_t size) {
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   return xRealloc(ptr, nmemb * size);
}

void* xReallocArrayZero(void* ptr, size_t prevmemb, size_t newmemb, size_t size) {
   assert((ptr == NULL) == (prevmemb == 0));

   if (prevmemb == newmemb) {
      return ptr;
   }

   void* ret = xReallocArray(ptr, newmemb, size);

   if (newmemb > prevmemb) {
      memset((unsigned char*)ret + prevmemb * size, '\0', (newmemb - prevmemb) * size);
   }

   return ret;
}

inline bool String_contains_i(const char* s1, const char* s2, bool multi) {
   // we have a multi-string search term, handle as special case for performance reasons
   if (multi && strstr(s2, "|")) {
      size_t nNeedles;
      char** needles = String_split(s2, '|', &nNeedles);
      for (size_t i = 0; i < nNeedles; i++) {
         if (strcasestr(s1, needles[i]) != NULL) {
            String_freeArray(needles);
            return true;
         }
      }
      String_freeArray(needles);
      return false;
   } else {
      return strcasestr(s1, s2) != NULL;
   }
}

char* String_cat(const char* s1, const char* s2) {
   const size_t l1 = strlen(s1);
   const size_t l2 = strlen(s2);
   if (SIZE_MAX - l1 <= l2) {
      fail();
   }
   char* out = xMalloc(l1 + l2 + 1);
   memcpy(out, s1, l1);
   memcpy(out + l1, s2, l2);
   out[l1 + l2] = '\0';
   return out;
}

char* String_trim(const char* in) {
   while (in[0] == ' ' || in[0] == '\t' || in[0] == '\n') {
      in++;
   }

   size_t len = strlen(in);
   while (len > 0 && (in[len - 1] == ' ' || in[len - 1] == '\t' || in[len - 1] == '\n')) {
      len--;
   }

   return xStrndup(in, len);
}

char** String_split(const char* s, char sep, size_t* n) {
   const size_t rate = 10;
   char** out = xCalloc(rate, sizeof(char*));
   size_t ctr = 0;
   size_t blocks = rate;
   const char* where;
   while ((where = strchr(s, sep)) != NULL) {
      size_t size = (size_t)(where - s);
      out[ctr] = xStrndup(s, size);
      ctr++;
      if (ctr == blocks) {
         blocks += rate;
         out = (char**) xRealloc(out, sizeof(char*) * blocks);
      }
      s += size + 1;
   }
   if (s[0] != '\0') {
      out[ctr] = xStrdup(s);
      ctr++;
   }
   out = xRealloc(out, sizeof(char*) * (ctr + 1));
   out[ctr] = NULL;

   if (n)
      *n = ctr;

   return out;
}

void String_freeArray(char** s) {
   if (!s) {
      return;
   }
   for (size_t i = 0; s[i] != NULL; i++) {
      free(s[i]);
   }
   free(s);
}

char* String_readLine(FILE* fd) {
   const size_t step = 1024;
   size_t bufSize = step;
   char* buffer = xMalloc(step + 1);
   char* at = buffer;
   for (;;) {
      const char* ok = fgets(at, step + 1, fd);
      if (!ok) {
         free(buffer);
         return NULL;
      }
      char* newLine = strrchr(at, '\n');
      if (newLine) {
         *newLine = '\0';
         return buffer;
      } else {
         if (feof(fd)) {
            return buffer;
         }
      }
      bufSize += step;
      buffer = xRealloc(buffer, bufSize + 1);
      at = buffer + bufSize - step;
   }
}

size_t String_safeStrncpy(char* restrict dest, const char* restrict src, size_t size) {
   assert(size > 0);

   size_t i = 0;
   for (; i < size - 1 && src[i]; i++)
      dest[i] = src[i];

   dest[i] = '\0';

   return i;
}

int xAsprintf(char** strp, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int r = vasprintf(strp, fmt, vl);
   va_end(vl);

   if (r < 0 || !*strp) {
      fail();
   }

   return r;
}

int xSnprintf(char* buf, size_t len, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int n = vsnprintf(buf, len, fmt, vl);
   va_end(vl);

   if (n < 0 || (size_t)n >= len) {
      fail();
   }

   return n;
}

char* xStrdup(const char* str) {
   char* data = strdup(str);
   if (!data) {
      fail();
   }
   return data;
}

void free_and_xStrdup(char** ptr, const char* str) {
   if (*ptr && String_eq(*ptr, str))
      return;

   free(*ptr);
   *ptr = xStrdup(str);
}

char* xStrndup(const char* str, size_t len) {
   char* data = strndup(str, len);
   if (!data) {
      fail();
   }
   return data;
}

ATTR_ACCESS3_W(2, 3)
static ssize_t readfd_internal(int fd, void* buffer, size_t count) {
   if (!count) {
      close(fd);
      return -EINVAL;
   }

   ssize_t alreadyRead = 0;
   count--; // reserve one for null-terminator

   for (;;) {
      ssize_t res = read(fd, buffer, count);
      if (res == -1) {
         if (errno == EINTR)
            continue;

         close(fd);
         *((char*)buffer) = '\0';
         return -errno;
      }

      if (res > 0) {
         assert((size_t)res <= count);

         buffer = ((char*)buffer) + res;
         count -= (size_t)res;
         alreadyRead += res;
      }

      if (count == 0 || res == 0) {
         close(fd);
         *((char*)buffer) = '\0';
         return alreadyRead;
      }
   }
}

ssize_t xReadfile(const char* pathname, void* buffer, size_t count) {
   int fd = open(pathname, O_RDONLY);
   if (fd < 0)
      return -errno;

   return readfd_internal(fd, buffer, count);
}

ssize_t xReadfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count) {
   int fd = Compat_openat(dirfd, pathname, O_RDONLY);
   if (fd < 0)
      return -errno;

   return readfd_internal(fd, buffer, count);
}

ssize_t full_write(int fd, const void* buf, size_t count) {
   ssize_t written = 0;

   while (count > 0) {
      ssize_t r = write(fd, buf, count);
      if (r < 0) {
         if (errno == EINTR)
            continue;

         return r;
      }

      if (r == 0)
         break;

      written += r;
      buf = (const unsigned char*)buf + r;
      count -= (size_t)r;
   }

   return written;
}

/* Compares floating point values for ordering data entries. In this function,
   NaN is considered "less than" any other floating point value (regardless of
   sign), and two NaNs are considered "equal" regardless of payload. */
int compareRealNumbers(double a, double b) {
   int result = isgreater(a, b) - isgreater(b, a);
   if (result)
      return result;
   return !isNaN(a) - !isNaN(b);
}

/* Computes the sum of all positive floating point values in an array.
   NaN values in the array are skipped. The returned sum will always be
   nonnegative. */
double sumPositiveValues(const double* array, size_t count) {
   double sum = 0.0;
   for (size_t i = 0; i < count; i++) {
      if (isPositive(array[i]))
         sum += array[i];
   }
   return sum;
}
