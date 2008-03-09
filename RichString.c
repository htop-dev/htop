
#include "RichString.h"

#ifndef CONFIG_H
#define CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "debug.h"
#include <assert.h>
#ifdef HAVE_LIBNCURSESW
#include <wchar.h>
#endif

#define RICHSTRING_MAXLEN 300

/*{

#define RichString_init(this) (this)->len = 0
#define RichString_initVal(this) (this).len = 0

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, this.chstr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, this.chstr + off, n)
#define RichString_getCharVal(this, i) (this.chstr[i].chars[0] & 255)
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, this.chstr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, this.chstr + off, n)
#define RichString_getCharVal(this, i) (this.chstr[i])
#endif

typedef struct RichString_ {
   int len;
#ifdef HAVE_LIBNCURSESW
   cchar_t chstr[RICHSTRING_MAXLEN+1];
#else
   chtype chstr[RICHSTRING_MAXLEN+1];
#endif
} RichString;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifdef HAVE_LIBNCURSESW

inline void RichString_appendn(RichString* this, int attrs, char* data_c, int len) {
   wchar_t data[RICHSTRING_MAXLEN];
   len = mbstowcs(data, data_c, RICHSTRING_MAXLEN);
   if (len<0)
      return;
   int last = MIN(RICHSTRING_MAXLEN - 1, len + this->len);
   for (int i = this->len, j = 0; i < last; i++, j++) {
      memset(&this->chstr[i], 0, sizeof(this->chstr[i]));
      this->chstr[i].chars[0] = data[j];
      this->chstr[i].attr = attrs;
   }
   this->chstr[last].chars[0] = 0;
   this->len = last;
}

inline void RichString_setAttrn(RichString *this, int attrs, int start, int finish) {
   cchar_t* ch = this->chstr + start;
   for (int i = start; i <= finish; i++) {
      ch->attr = attrs;
      ch++;
   }
}

int RichString_findChar(RichString *this, char c, int start) {
   wchar_t wc = btowc(c);
   cchar_t* ch = this->chstr + start;
   for (int i = start; i < this->len; i++) {
      if (ch->chars[0] == wc)
         return i;
      ch++;
   }
   return -1;
}

#else

inline void RichString_appendn(RichString* this, int attrs, char* data_c, int len) {
   int last = MIN(RICHSTRING_MAXLEN - 1, len + this->len);
   for (int i = this->len, j = 0; i < last; i++, j++)
      this->chstr[i] = data_c[j] | attrs;
   this->chstr[last] = 0;
   this->len = last;
}

void RichString_setAttrn(RichString *this, int attrs, int start, int finish) {
   chtype* ch = this->chstr + start;
   for (int i = start; i <= finish; i++) {
      *ch = (*ch & 0xff) | attrs;
      ch++;
   }
}

int RichString_findChar(RichString *this, char c, int start) {
   chtype* ch = this->chstr + start;
   for (int i = start; i < this->len; i++) {
      if ((*ch & 0xff) == c)
         return i;
      ch++;
   }
   return -1;
}

#endif

void RichString_setAttr(RichString *this, int attrs) {
   RichString_setAttrn(this, attrs, 0, this->len - 1);
}

inline void RichString_append(RichString* this, int attrs, char* data) {
   RichString_appendn(this, attrs, data, strlen(data));
}

void RichString_write(RichString* this, int attrs, char* data) {
   RichString_init(this);
   RichString_append(this, attrs, data);
}

RichString RichString_quickString(int attrs, char* data) {
   RichString str;
   RichString_initVal(str);
   RichString_write(&str, attrs, data);
   return str;
}
