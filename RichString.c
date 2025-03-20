/*
htop - RichString.c
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RichString.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h> // IWYU pragma: keep
#include <stdlib.h>
#include <string.h>

#include "Macros.h"
#include "XUtils.h"


#define charBytes(n) (sizeof(CharType) * (n))

static void RichString_extendLen(RichString* this, size_t len) {
   if (this->chptr == this->chstr) {
      // String is in internal buffer
      if (len > RICHSTRING_MAXLEN) {
         // Copy from internal buffer to allocated string
         this->chptr = xMalloc(charBytes(len + 1));
         memcpy(this->chptr, this->chstr, charBytes(this->chlen));
      } else {
         // Still fits in internal buffer, do nothing
         assert(this->chlen <= RICHSTRING_MAXLEN);
      }
   } else {
      // String is managed externally
      if (len > RICHSTRING_MAXLEN) {
         // Just reallocate the buffer accordingly
         this->chptr = xRealloc(this->chptr, charBytes(len + 1));
      } else {
         // Move string into internal buffer and free resources
         memcpy(this->chstr, this->chptr, charBytes(len));
         free(this->chptr);
         this->chptr = this->chstr;
      }
   }

   RichString_setChar(this, len, 0);
   this->chlen = len;
}

static void RichString_setLen(RichString* this, size_t len) {
   if (len <= RICHSTRING_MAXLEN && this->chlen <= RICHSTRING_MAXLEN) {
      RichString_setChar(this, len, 0);
      this->chlen = len;
   } else {
      RichString_extendLen(this, len);
   }
}

void RichString_rewind(RichString* this, size_t count) {
   RichString_setLen(this, this->chlen > count ? this->chlen - count : 0);
}

#ifdef HAVE_LIBNCURSESW

static size_t mbstowcs_nonfatal(wchar_t* restrict dest, const char* restrict src, size_t n) {
   size_t written = 0;
   mbstate_t ps = { 0 };
   bool broken = false;

   while (written < n) {
      size_t ret = mbrtowc(dest, src, SIZE_MAX, &ps);
      if (ret == (size_t)-1 || ret == (size_t)-2) {
         if (!broken) {
            broken = true;
            *dest++ = L'\xFFFD';
            written++;
         }
         src++;
         continue;
      }

      broken = false;

      if (ret == 0) {
         break;
      }

      dest++;
      written++;
      src += ret;
   }

   return written;
}

static inline size_t RichString_writeFromWide(RichString* this, int attrs, const char* data_c, size_t from, size_t len) {
   wchar_t data[len];
   len = mbstowcs_nonfatal(data, data_c, len);
   if (len <= 0)
      return 0;

   size_t newLen = from + len;
   RichString_setLen(this, newLen);
   for (size_t i = from, j = 0; i < newLen; i++, j++) {
      this->chptr[i] = (CharType) { .attr = attrs & 0xffffff, .chars = { (iswprint(data[j]) ? data[j] : L'\xFFFD') } };
   }

   return len;
}

size_t RichString_appendnWideColumns(RichString* this, int attrs, const char* data_c, size_t len, int* columns) {
   wchar_t data[len];
   len = mbstowcs_nonfatal(data, data_c, len);
   if (len <= 0)
      return 0;

   size_t from = this->chlen;
   size_t newLen = from + len;
   RichString_setLen(this, newLen);
   int columnsWritten = 0;
   size_t pos = from;
   for (size_t j = 0; j < len; j++) {
      wchar_t c = iswprint(data[j]) ? data[j] : L'\xFFFD';
      int cwidth = wcwidth(c);
      if (cwidth > *columns)
         break;

      *columns -= cwidth;
      columnsWritten += cwidth;

      this->chptr[pos] = (CharType) { .attr = attrs & 0xffffff, .chars = { c, '\0' } };
      pos++;
   }

   RichString_setLen(this, pos);
   *columns = columnsWritten;

   return pos - from;
}

static inline size_t RichString_writeFromAscii(RichString* this, int attrs, const char* data, size_t from, size_t len) {
   size_t newLen = from + len;
   RichString_setLen(this, newLen);
   for (size_t i = from, j = 0; i < newLen; i++, j++) {
      assert((unsigned char)data[j] <= SCHAR_MAX);
      this->chptr[i] = (CharType) { .attr = attrs & 0xffffff, .chars = { (isprint((unsigned char)data[j]) ? data[j] : L'\xFFFD') } };
   }

   return len;
}

inline void RichString_setAttrn(RichString* this, int attrs, size_t start, size_t charcount) {
   size_t end = CLAMP(start + charcount, 0, this->chlen);
   for (size_t i = start; i < end; i++) {
      this->chptr[i].attr = attrs;
   }
}

void RichString_appendChr(RichString* this, int attrs, char c, size_t count) {
   size_t from = this->chlen;
   size_t newLen = from + count;
   RichString_setLen(this, newLen);
   for (size_t i = from; i < newLen; i++) {
      this->chptr[i] = (CharType) { .attr = attrs, .chars = { c, 0 } };
   }
}

size_t RichString_findChar(const RichString* this, char c, size_t start) {
   const wchar_t wc = btowc(c);
   const cchar_t* ch = this->chptr + start;
   for (size_t i = start; i < this->chlen; i++) {
      if (ch->chars[0] == wc)
         return i;
      ch++;
   }
   return (size_t)-1;
}

#else /* HAVE_LIBNCURSESW */

static inline size_t RichString_writeFromWide(RichString* this, int attrs, const char* data_c, size_t from, size_t len) {
   size_t newLen = from + len;
   RichString_setLen(this, newLen);
   for (size_t i = from, j = 0; i < newLen; i++, j++) {
      this->chptr[i] = (((unsigned char)data_c[j]) >= 32 ? ((unsigned char)data_c[j]) : '?') | attrs;
   }
   this->chptr[newLen] = 0;

   return len;
}

size_t RichString_appendnWideColumns(RichString* this, int attrs, const char* data_c, size_t len, int* columns) {
   size_t written = RichString_writeFromWide(this, attrs, data_c, this->chlen, MINIMUM(len, (unsigned int)*columns));
   *columns = (int)written;
   return written;
}

static inline size_t RichString_writeFromAscii(RichString* this, int attrs, const char* data_c, size_t from, size_t len) {
   return RichString_writeFromWide(this, attrs, data_c, from, len);
}

void RichString_setAttrn(RichString* this, int attrs, size_t start, size_t charcount) {
   size_t end = CLAMP(start + charcount, 0, this->chlen);
   for (size_t i = start; i < end; i++) {
      this->chptr[i] = (this->chptr[i] & 0xff) | attrs;
   }
}

void RichString_appendChr(RichString* this, int attrs, char c, size_t count) {
   size_t from = this->chlen;
   size_t newLen = from + count;
   RichString_setLen(this, newLen);
   for (size_t i = from; i < newLen; i++) {
      this->chptr[i] = c | attrs;
   }
}

size_t RichString_findChar(const RichString* this, char c, size_t start) {
   const chtype* ch = this->chptr + start;
   for (size_t i = start; i < this->chlen; i++) {
      if ((*ch & 0xff) == (chtype) c)
         return i;
      ch++;
   }
   return (size_t)-1;
}

#endif /* HAVE_LIBNCURSESW */

void RichString_delete(RichString* this) {
   if (this->chlen > RICHSTRING_MAXLEN) {
      free(this->chptr);
      this->chptr = this->chstr;
   }
}

void RichString_setAttr(RichString* this, int attrs) {
   RichString_setAttrn(this, attrs, 0, this->chlen);
}

size_t RichString_appendWide(RichString* this, int attrs, const char* data) {
   return RichString_writeFromWide(this, attrs, data, this->chlen, strlen(data));
}

size_t RichString_appendnWide(RichString* this, int attrs, const char* data, size_t len) {
   return RichString_writeFromWide(this, attrs, data, this->chlen, len);
}

size_t RichString_writeWide(RichString* this, int attrs, const char* data) {
   return RichString_writeFromWide(this, attrs, data, 0, strlen(data));
}

size_t RichString_appendAscii(RichString* this, int attrs, const char* data) {
   return RichString_writeFromAscii(this, attrs, data, this->chlen, strlen(data));
}

size_t RichString_appendnAscii(RichString* this, int attrs, const char* data, size_t len) {
   return RichString_writeFromAscii(this, attrs, data, this->chlen, len);
}

size_t RichString_writeAscii(RichString* this, int attrs, const char* data) {
   return RichString_writeFromAscii(this, attrs, data, 0, strlen(data));
}
