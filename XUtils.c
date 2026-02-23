/*
htop - StringUtils.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "XUtils.h"

#include <assert.h>
#include <ctype.h> // IWYU pragma: keep
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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
   void* data = realloc(ptr, size);
   if (!data) {
      /* free'ing ptr here causes an indirect memory leak if pointers
       * are held as part of an potential array referenced in ptr.
       * In GCC 14 -fanalyzer recognizes this leak, but fails to
       * ignore it given that this path ends in a noreturn function.
       * Thus to avoid this confusing diagnostic we opt to leave
       * that pointer alone instead.
       */
      // free(ptr);
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

char* String_readLine(FILE* fp) {
   const size_t step = 1024;
   size_t bufSize = step;
   char* buffer = xMalloc(step + 1);
   char* at = buffer;
   for (;;) {
      const char* ok = fgets(at, step + 1, fp);
      if (!ok) {
         free(buffer);
         return NULL;
      }
      char* newLine = strrchr(at, '\n');
      if (newLine) {
         *newLine = '\0';
         return buffer;
      } else {
         if (feof(fp)) {
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

#ifndef HAVE_STRNLEN
size_t strnlen(const char* str, size_t maxLen) {
   for (size_t len = 0; len < maxLen; len++) {
      if (!str[len]) {
         return len;
      }
   }
   return maxLen;
}
#endif

#ifdef HAVE_LIBNCURSESW
static void String_encodeWChar(WCharEncoderState* ps, wchar_t wc) {
   assert(!ps->buf || ps->pos < ps->size);

   char tempBuf[MB_LEN_MAX];
   char* dest = ps->buf ? (char*)ps->buf + ps->pos : tempBuf;

   // It is unnecessarily expensive to fix the output string if the caller
   // gives an incorrect buffer size. This function would not support any
   // truncation of the output string.
   size_t len = wcrtomb(dest, wc, &ps->mbState);
   assert(len > 0);
   if (len == (size_t)-1) {
      assert(len != (size_t)-1);
      fail();
   }
   if (ps->buf && len > ps->size - ps->pos) {
      assert(!ps->buf || len <= ps->size - ps->pos);
      fail();
   }

   ps->pos += len;
}
#else
static void String_encodeWChar(WCharEncoderState* ps, int c) {
   assert(!ps->buf || ps->pos < ps->size);

   char* buf = ps->buf;
   if (buf)
      buf[ps->pos] = (char)c;

   ps->pos += 1;
}
#endif

void EncodePrintableString(WCharEncoderState* ps, const char* src, size_t maxLen, EncodeWChar encodeWChar) {
   assert(src || maxLen == 0);

   size_t pos = 0;
   bool wasReplaced = false;

#ifdef HAVE_LIBNCURSESW
   const wchar_t replacementChar = CRT_utf8 ? L'\xFFFD' : L'?';
   wchar_t ch;

   mbstate_t decState;
   memset(&decState, 0, sizeof(decState));
#else
   const char replacementChar = '?';
   char ch;
#endif

   do {
      size_t len = 0;
      bool shouldReplace = false;
      ch = 0;

      if (pos < maxLen) {
         // Read the next character from the byte sequence
#ifdef HAVE_LIBNCURSESW
         mbstate_t newState;
         memcpy(&newState, &decState, sizeof(newState));
         len = mbrtowc(&ch, &src[pos], maxLen - pos, &newState);

         assert(len != 0 || ch == 0);
         switch (len) {
         case (size_t)-2:
            errno = EILSEQ;
            shouldReplace = true;
            len = maxLen - pos;
            break;

         case (size_t)-1:
            shouldReplace = true;
            len = 1;
            break;

         default:
            memcpy(&decState, &newState, sizeof(decState));
         }
#else
         len = 1;
         ch = src[pos];
#endif
      }

      pos += len;

      // Filter unprintable characters
      if (!shouldReplace && ch != 0) {
#ifdef HAVE_LIBNCURSESW
         shouldReplace = !iswprint(ch);
#else
         shouldReplace = !isprint((unsigned char)ch);
#endif
      }

      if (shouldReplace) {
         ch = replacementChar;
         if (wasReplaced)
            continue;
      }
      wasReplaced = shouldReplace;

      encodeWChar(ps, ch);
   } while (ch != 0);
}

char* String_makePrintable(const char* str, size_t maxLen) {
   WCharEncoderState encState;

   memset(&encState, 0, sizeof(encState));
   EncodePrintableString(&encState, str, maxLen, String_encodeWChar);
   size_t size = encState.pos;
   assert(size > 0);

   memset(&encState, 0, sizeof(encState));
   char* buf = xMalloc(size);
   encState.size = size;
   encState.buf = buf;
   EncodePrintableString(&encState, str, maxLen, String_encodeWChar);
   assert(encState.pos == size);

   return buf;
}

bool String_decodeNextWChar(MBStringDecoderState* ps) {
   if (!ps->str || ps->maxLen == 0)
      return false;

   // If the previous call of this function encounters an invalid sequence,
   // do not continue (because the "mbState" object for mbrtowc() is
   // undefined). The caller is supposed to reset the state.
#ifdef HAVE_LIBNCURSESW
   bool isStateDefined = ps->ch != WEOF;
#else
   bool isStateDefined = ps->ch != EOF;
#endif
   if (!isStateDefined)
      return false;

#ifdef HAVE_LIBNCURSESW
   wchar_t wc;
   size_t len = mbrtowc(&wc, ps->str, ps->maxLen, &ps->mbState);
   switch (len) {
   case (size_t)-1:
      // Invalid sequence
      ps->ch = WEOF;
      return false;

   case (size_t)-2:
      // Incomplete sequence
      ps->str += ps->maxLen;
      ps->maxLen = 0;
      return false;

   case 0:
      assert(wc == 0);

      ps->str = NULL;
      ps->maxLen = 0;
      ps->ch = wc;
      return true;

   default:
      ps->str += len;
      ps->maxLen -= len;
      ps->ch = wc;
   }
   return true;
#else
   const size_t len = 1;
   ps->ch = *ps->str;
   if (ps->ch == 0) {
      ps->str = NULL;
      ps->maxLen = 0;
   } else {
      ps->str += len;
      ps->maxLen -= len;
   }
   return true;
#endif
}

int String_lineBreakWidth(const char** str, size_t maxLen, int maxWidth, char separator) {
   assert(*str || maxLen == 0);

   // The caller should ensure (maxWidth >= 0).
   // It's possible for a Unicode string to occupy 0 terminal columns, so this
   // function allows (maxWidth == 0).
   if (maxWidth < 0)
      maxWidth = INT_MAX;

   MBStringDecoderState state;
   memset(&state, 0, sizeof(state));
   state.str = *str;
   state.maxLen = maxLen;

   int totalWidth = 0;
   int breakWidth = 0;

   const char* breakPos = NULL;
   bool inSpaces = true;

   while (String_decodeNextWChar(&state)) {
      if (state.ch == 0)
         break;

      if (state.ch == ' ' && separator == ' ' && !inSpaces) {
         breakWidth = totalWidth;
         breakPos = *str;
         inSpaces = true;
      }

#ifdef HAVE_LIBNCURSESW
      int cw = wcwidth((wchar_t)state.ch);
      if (cw < 0) {
         // This function should not be used with string containing unprintable
         // characters. Tolerate them on release build, however.
         assert(cw >= 0);
         break;
      }
#else
      assert(isprint(state.ch));
      const int cw = 1;
#endif

      if (cw > maxWidth - totalWidth) {
         // This character cannot fit the line with the given maxWidth.
         if (breakPos) {
            // Rewind the scanning state to the last found separator.
            totalWidth = breakWidth;
            *str = breakPos;
         }
         break;
      }

#ifdef HAVE_LIBNCURSESW
      // If the character takes zero columns, include the character in the
      // substring if the working encoding is UTF-8, and ignore it otherwise.
      // In Unicode, combining characters are always placed after the base
      // character, but some legacy 8-bit encodings instead place combining
      // characters before the base character.
      if (cw <= 0 && !CRT_utf8)
         continue;
#endif

      totalWidth += cw;

      // (*str - start) will represent the length of the substring bounded
      // by the width limit.
      *str = state.str;

      if (state.ch != ' ')
         inSpaces = false;

#ifdef HAVE_LIBNCURSESW
      bool isSeparator = state.ch == (wint_t)separator;
#else
      bool isSeparator = state.ch == (int)separator;
#endif
      if (isSeparator && separator != ' ') {
         breakWidth = totalWidth;
         breakPos = *str;
      }
   }

   return totalWidth;
}

int String_mbswidth(const char** str, size_t maxLen, int maxWidth) {
#ifdef HAVE_LIBNCURSESW
   return String_lineBreakWidth(str, maxLen, maxWidth, '\0');
#else
   assert(*str || maxLen == 0);

   if (maxWidth < 0)
      maxWidth = INT_MAX;

   maxLen = MINIMUM((size_t)maxWidth, maxLen);
   size_t len = strnlen(*str, maxLen);
   *str += len;
   return (int)len;
#endif
}

int xAsprintf(char** strp, const char* fmt, ...) {
   *strp = NULL;

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
   assert(len > 0);

   // POSIX says snprintf() can fail if (len > INT_MAX).
   len = MINIMUM(INT_MAX, len);

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

/* Counts the number of digits needed to print "n" with a given base.
   If "n" is zero, returns 1. This function expects small numbers to
   appear often, hence it uses a O(log(n)) time algorithm. */
size_t countDigits(size_t n, size_t base) {
   assert(base > 1);
   size_t res = 1;
   for (size_t limit = base; n >= limit; limit *= base) {
      res++;
      if (base && limit > SIZE_MAX / base) {
         break;
      }
   }
   return res;
}

#if !defined(HAVE_BUILTIN_CTZ)
// map a bit value mod 37 to its position
static const uint8_t mod37BitPosition[] = {
  32, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4,
  7, 17, 0, 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5,
  20, 8, 19, 18
};

/* Returns the number of trailing zero bits */
unsigned int countTrailingZeros(unsigned int x) {
   return mod37BitPosition[(-x & x) % 37];
}
#endif
