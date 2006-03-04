
#include "RichString.h"

#include <stdlib.h>
#include <string.h>
#include <curses.h>

#include "debug.h"
#include <assert.h>

#define RICHSTRING_MAXLEN 300

/*{

typedef struct RichString_ {
   int len;
   chtype chstr[RICHSTRING_MAXLEN+1];
} RichString;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* private property */
WINDOW* workArea = NULL;

RichString RichString_new() {
   RichString this;
   this.len = 0;
   return this;
}

void RichString_delete(RichString this) {
}

void RichString_prune(RichString* this) {
   this->len = 0;
}

void RichString_write(RichString* this, int attrs, char* data) {
   this->len = 0;
   RichString_append(this, attrs, data);
}

inline void RichString_append(RichString* this, int attrs, char* data) {
   RichString_appendn(this, attrs, data, strlen(data));
}

inline void RichString_appendn(RichString* this, int attrs, char* data, int len) {
   if (!workArea) {
      workArea = newpad(1, RICHSTRING_MAXLEN);
   }
   assert(workArea);
   wattrset(workArea, attrs);
   int maxToWrite = (RICHSTRING_MAXLEN - 1) - this->len;
   int wrote = MIN(maxToWrite, len);
   mvwaddnstr(workArea, 0, 0, data, maxToWrite);
   int oldstrlen = this->len;
   this->len += wrote;
   mvwinchnstr(workArea, 0, 0, this->chstr + oldstrlen, wrote);
   wattroff(workArea, attrs);
}

void RichString_setAttr(RichString *this, int attrs) {
   for (int i = 0; i < this->len; i++) {
      char c = this->chstr[i];
      this->chstr[i] = c | attrs;
   }
}

void RichString_applyAttr(RichString *this, int attrs) {
   for (int i = 0; i < this->len - 1; i++) {
      this->chstr[i] |= attrs;
   }
}

RichString RichString_quickString(int attrs, char* data) {
   RichString str = RichString_new();
   RichString_write(&str, attrs, data);
   return str;
}
