/*
htop - RichString.c
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "RichString.h"

#include <stdlib.h>
#include <string.h>

#define RICHSTRING_MAXLEN 300

/*{
#include "config.h"
#include <ctype.h>

#include <assert.h>
#ifdef HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#define RichString_size(this) ((this)->chlen)
#define RichString_sizeVal(this) ((this).chlen)

#define RichString_begin(this) RichString (this); (this).chlen = 0; (this).chptr = (this).chstr;
#define RichString_beginAllocated(this) (this).chlen = 0; (this).chptr = (this).chstr;
#define RichString_end(this) RichString_prune(&(this));

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i].chars[0] & 255)
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)].chars[0] = ch; } while(0)
#define CharType cchar_t
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i])
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)] = ch; } while(0)
#define CharType chtype
#endif

typedef struct RichString_ {
   int chlen;
   CharType chstr[RICHSTRING_MAXLEN+1];
   CharType* chptr;
} RichString;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define charBytes(n) (sizeof(CharType) * (n)) 

static inline void RichString_setLen(RichString* this, int len) {
   if (this->chlen <= RICHSTRING_MAXLEN) {
      if (len > RICHSTRING_MAXLEN) {
         this->chptr = malloc(charBytes(len+1));
         memcpy(this->chptr, this->chstr, charBytes(this->chlen+1));
      }
   } else {
      if (len <= RICHSTRING_MAXLEN) {
         memcpy(this->chstr, this->chptr, charBytes(this->chlen));
         free(this->chptr);
         this->chptr = this->chstr;
      } else {
         this->chptr = realloc(this->chptr, charBytes(len+1));
      }
   }
   RichString_setChar(this, len, 0);
   this->chlen = len;
}

#ifdef HAVE_LIBNCURSESW

inline void RichString_appendn(RichString* this, int attrs, const char* data_c, int len) {
   wchar_t data[len+1];
   len = mbstowcs(data, data_c, len);
   if (len<0)
      return;
   int oldLen = this->chlen;
   int newLen = len + oldLen;
   RichString_setLen(this, newLen);
   for (int i = oldLen, j = 0; i < newLen; i++, j++) {
      memset(&this->chptr[i], 0, sizeof(this->chptr[i]));
      this->chptr[i].chars[0] = data[j];
      this->chptr[i].attr = attrs;
   }
   this->chptr[newLen].chars[0] = 0;
}

inline void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   cchar_t* ch = this->chptr + start;
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

inline void RichString_appendn(RichString* this, int attrs, const char* data_c, int len) {
   int oldLen = this->chlen;
   int newLen = len + oldLen;
   RichString_setLen(this, newLen);
   for (int i = oldLen, j = 0; i < newLen; i++, j++)
      this->chptr[i] = (isprint(data_c[j]) ? data_c[j] : '?') | attrs;
   this->chptr[newLen] = 0;
}

void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   chtype* ch = this->chptr + start;
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
   this->chptr = this->chstr;
   this->chlen = 0;
}

void RichString_setAttr(RichString* this, int attrs) {
   RichString_setAttrn(this, attrs, 0, this->chlen - 1);
}

inline void RichString_append(RichString* this, int attrs, const char* data) {
   RichString_appendn(this, attrs, data, strlen(data));
}

void RichString_write(RichString* this, int attrs, const char* data) {
   RichString_setLen(this, 0);
   RichString_appendn(this, attrs, data, strlen(data));
}
