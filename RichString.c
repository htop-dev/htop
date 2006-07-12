
#include "RichString.h"

#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "debug.h"
#include <assert.h>

#define RICHSTRING_MAXLEN 300

/*{

#define RichString_init(this) (this)->len = 0
#define RichString_initVal(this) (this).len = 0

typedef struct RichString_ {
   int len;
   chtype chstr[RICHSTRING_MAXLEN+1];
} RichString;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

void RichString_write(RichString* this, int attrs, char* data) {
   RichString_init(this);
   RichString_append(this, attrs, data);
}

inline void RichString_append(RichString* this, int attrs, char* data) {
   RichString_appendn(this, attrs, data, strlen(data));
}

inline void RichString_appendn(RichString* this, int attrs, char* data, int len) {
   int last = MIN(RICHSTRING_MAXLEN - 1, len + this->len);
   for (int i = this->len, j = 0; i < last; i++, j++)
      this->chstr[i] = data[j] | attrs;
   this->chstr[last] = 0;
   this->len = last;
}

void RichString_setAttr(RichString *this, int attrs) {
   chtype* ch = this->chstr;
   for (int i = 0; i < this->len; i++) {
      *ch = (*ch & 0xff) | attrs;
      ch++;
   }
}

void RichString_applyAttr(RichString *this, int attrs) {
   chtype* ch = this->chstr;
   for (int i = 0; i < this->len; i++) {
      *ch |= attrs;
      ch++;
   }
}

RichString RichString_quickString(int attrs, char* data) {
   RichString str;
   RichString_initVal(str);
   RichString_write(&str, attrs, data);
   return str;
}
