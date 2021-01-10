#ifndef HEADER_RichString
#define HEADER_RichString
/*
htop - RichString.h
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "ProvideCurses.h"


#define RichString_size(this) ((this)->chlen)
#define RichString_sizeVal(this) ((this).chlen)

#define RichString_begin(this) RichString (this); RichString_beginAllocated(this)
#define RichString_beginAllocated(this) do { memset(&(this), 0, sizeof(RichString)); (this).chptr = (this).chstr; } while(0)
#define RichString_end(this) RichString_prune(&(this))

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, (this).chptr + (off), n)
#define RichString_getCharVal(this, i) ((this).chptr[i].chars[0] & 255)
#define RichString_setChar(this, at, ch) do { (this)->chptr[(at)] = (CharType) { .chars = { ch, 0 } }; } while (0)
#define CharType cchar_t
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, (this).chptr + (off), n)
#define RichString_getCharVal(this, i) ((this).chptr[i])
#define RichString_setChar(this, at, ch) do { (this)->chptr[(at)] = ch; } while (0)
#define CharType chtype
#endif

#define RICHSTRING_MAXLEN 350

typedef struct RichString_ {
   int chlen;
   CharType* chptr;
   CharType chstr[RICHSTRING_MAXLEN + 1];
   int highlightAttr;
} RichString;

void RichString_setAttrn(RichString* this, int attrs, int start, int charcount);

int RichString_findChar(RichString* this, char c, int start);

void RichString_prune(RichString* this);

void RichString_setAttr(RichString* this, int attrs);

void RichString_appendChr(RichString* this, char c, int count);

int RichString_appendWide(RichString* this, int attrs, const char* data);

int RichString_appendnWide(RichString* this, int attrs, const char* data, int len);

int RichString_writeWide(RichString* this, int attrs, const char* data);

int RichString_appendAscii(RichString* this, int attrs, const char* data);

int RichString_appendnAscii(RichString* this, int attrs, const char* data, int len);

int RichString_writeAscii(RichString* this, int attrs, const char* data);

#endif
