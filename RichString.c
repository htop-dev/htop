/*
htop - RichString.c
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "RichString.h"
#include "XAlloc.h"
#include "Macros.h"

#include <stdlib.h>
#include <string.h>

#define charBytes(n) (sizeof(CharType) * (n))

static void RichString_extendLen(RichString* this, int len) {
   if (this->chlen <= RICHSTRING_MAXLEN) {
      if (len > RICHSTRING_MAXLEN) {
         this->chptr = xMalloc(charBytes(len + 1));
         memcpy(this->chptr, this->chstr, charBytes(this->chlen));
      }
   } else {
      if (len <= RICHSTRING_MAXLEN) {
         memcpy(this->chstr, this->chptr, charBytes(len));
         free(this->chptr);
         this->chptr = this->chstr;
      } else {
         this->chptr = xRealloc(this->chptr, charBytes(len + 1));
      }
   }

   RichString_setChar(this, len, 0);
   this->chlen = len;
}

#define RichString_setLen(this, len) do{ if(len < RICHSTRING_MAXLEN && this->chlen < RICHSTRING_MAXLEN) { RichString_setChar(this,len,0); this->chlen=len; } else RichString_extendLen(this,len); }while(0)

#ifdef HAVE_LIBNCURSESW

static inline void RichString_writeFrom(RichString* this, int attrs, const char* data_c, int from, int len) {
   wchar_t data[len+1];
   len = mbstowcs(data, data_c, len);
   if (len < 0)
      return;
   int newLen = from + len;
   RichString_setLen(this, newLen);
   for (int i = from, j = 0; i < newLen; i++, j++) {
      this->chptr[i] = (CharType) { .attr = attrs & 0xffffff, .chars = { (iswprint(data[j]) ? data[j] : '?') } };
   }
}

inline void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   cchar_t* ch = this->chptr + start;
   finish = CLAMP(finish, 0, this->chlen - 1);
   for (int i = start; i <= finish; i++) {
      ch->attr = attrs;
      ch++;
   }
}

int RichString_findChar(RichString* this, char c, int start) {
   wchar_t wc = btowc(c);
   cchar_t* ch = this->chptr + start;
   for (int i = start; i < this->chlen; i++) {
      if (ch->chars[0] == wc)
         return i;
      ch++;
   }
   return -1;
}

#else

static inline void RichString_writeFrom(RichString* this, int attrs, const char* data_c, int from, int len) {
   int newLen = from + len;
   RichString_setLen(this, newLen);
   for (int i = from, j = 0; i < newLen; i++, j++)
      this->chptr[i] = (data_c[j] >= 32 ? data_c[j] : '?') | attrs;
   this->chptr[newLen] = 0;
}

void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   chtype* ch = this->chptr + start;
   finish = CLAMP(finish, 0, this->chlen - 1);
   for (int i = start; i <= finish; i++) {
      *ch = (*ch & 0xff) | attrs;
      ch++;
   }
}

int RichString_findChar(RichString* this, char c, int start) {
   chtype* ch = this->chptr + start;
   for (int i = start; i < this->chlen; i++) {
      if ((*ch & 0xff) == (chtype) c)
         return i;
      ch++;
   }
   return -1;
}

#endif

void RichString_prune(RichString* this) {
   if (this->chlen > RICHSTRING_MAXLEN)
      free(this->chptr);
   memset(this, 0, sizeof(RichString));
   this->chptr = this->chstr;
}

void RichString_setAttr(RichString* this, int attrs) {
   RichString_setAttrn(this, attrs, 0, this->chlen - 1);
}

void RichString_append(RichString* this, int attrs, const char* data) {
   RichString_writeFrom(this, attrs, data, this->chlen, strlen(data));
}

void RichString_appendn(RichString* this, int attrs, const char* data, int len) {
   RichString_writeFrom(this, attrs, data, this->chlen, len);
}

void RichString_write(RichString* this, int attrs, const char* data) {
   RichString_writeFrom(this, attrs, data, 0, strlen(data));
}
