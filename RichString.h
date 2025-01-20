#ifndef HEADER_RichString
#define HEADER_RichString
/*
htop - RichString.h
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "Macros.h"
#include "ProvideCurses.h"


#define RichString_size(this) ((this)->chlen)
#define RichString_sizeVal(this) ((this).chlen)

#define RichString_begin(this) RichString this; RichString_beginAllocated(this)
#define RichString_beginAllocated(this)   \
   do {                                   \
      (this).chlen = 0;                   \
      (this).chptr = (this).chstr;        \
      RichString_setChar(&(this), 0, 0);  \
      (this).highlightAttr = 0;           \
   } while(0)

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, (this).chptr + (off), n)
#define RichString_getCharVal(this, i) ((this).chptr[i].chars[0])
#define RichString_setChar(this, at, ch) do { (this)->chptr[(at)] = (CharType) { .chars = { ch, 0 } }; } while (0)
#define CharType cchar_t
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, (this).chptr + (off), n)
#define RichString_getCharVal(this, i) ((this).chptr[i] & 0xff)
#define RichString_setChar(this, at, ch) do { (this)->chptr[(at)] = ch; } while (0)
#define CharType chtype
#endif

#define RICHSTRING_MAXLEN 350

typedef struct RichString_ {
   size_t chlen;
   CharType* chptr;
   CharType chstr[RICHSTRING_MAXLEN + 1];
   int highlightAttr;
} RichString;

void RichString_delete(RichString* this);

void RichString_rewind(RichString* this, size_t count);

void RichString_setAttrn(RichString* this, int attrs, size_t start, size_t charcount);

size_t RichString_findChar(const RichString* this, char c, size_t start);

void RichString_setAttr(RichString* this, int attrs);

void RichString_appendChr(RichString* this, int attrs, char c, size_t count);

/* All appending and writing functions return the number of written characters (not columns). */

size_t RichString_appendWide(RichString* this, int attrs, const char* data);

ATTR_ACCESS3_R(3, 4)
size_t RichString_appendnWide(RichString* this, int attrs, const char* data, size_t len);

/* columns takes the maximum number of columns to write and contains on return the number of columns written. */
size_t RichString_appendnWideColumns(RichString* this, int attrs, const char* data, size_t len, int* columns);

size_t RichString_writeWide(RichString* this, int attrs, const char* data);

size_t RichString_appendAscii(RichString* this, int attrs, const char* data);

ATTR_ACCESS3_R(3, 4)
size_t RichString_appendnAscii(RichString* this, int attrs, const char* data, size_t len);

size_t RichString_writeAscii(RichString* this, int attrs, const char* data);

ATTR_FORMAT(printf, 5, 6) ATTR_NONNULL_N(1, 3, 5)
size_t RichString_appendnFormatAscii(RichString* this, int attrs, char* buf, size_t len, const char* fmt, ...);

ATTR_FORMAT(printf, 3, 4) ATTR_NONNULL_N(1, 3)
size_t RichString_appendFormatAscii(RichString* this, int attrs, const char* fmt, ...);

#endif
