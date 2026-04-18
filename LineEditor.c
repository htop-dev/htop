/*
htop - LineEditor.c
(C) 2004-2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "LineEditor.h"

#include <ctype.h>
#include <string.h>

#include "CRT.h"
#include "Panel.h"
#include "ProvideCurses.h"


void LineEditor_init(LineEditor* this) {
   LineEditor_initWithMax(this, LINEEDITOR_MAX);
}

void LineEditor_initWithMax(LineEditor* this, size_t maxLen) {
   this->buffer[0] = '\0';
   this->len = 0;
   this->cursor = 0;
   this->scroll = 0;
   this->maxLen = (maxLen > 0 && maxLen <= LINEEDITOR_MAX) ? maxLen : LINEEDITOR_MAX;
}

void LineEditor_reset(LineEditor* this) {
   this->buffer[0] = '\0';
   this->len = 0;
   this->cursor = 0;
   this->scroll = 0;
}

void LineEditor_setText(LineEditor* this, const char* text) {
   if (!text)
      text = "";
   size_t copyLen = this->maxLen;
   strncpy(this->buffer, text, copyLen);
   this->buffer[copyLen] = '\0';
   this->len = strnlen(this->buffer, this->maxLen);
   this->cursor = this->len;
   this->scroll = 0;
}

/* Move cursor left by one character */
static inline void moveCursorLeft(LineEditor* this) {
   if (this->cursor > 0)
      this->cursor--;
}

/* Move cursor right by one character */
static inline void moveCursorRight(LineEditor* this) {
   if (this->cursor < this->len)
      this->cursor++;
}

/* Move cursor to the previous word boundary (Ctrl+Left / word-left) */
static void moveCursorWordLeft(LineEditor* this) {
   size_t pos = this->cursor;
   /* skip whitespace before cursor */
   while (pos > 0 && isspace((unsigned char)this->buffer[pos - 1]))
      pos--;
   /* skip non-whitespace (the word itself) */
   while (pos > 0 && !isspace((unsigned char)this->buffer[pos - 1]))
      pos--;
   this->cursor = pos;
}

/* Move cursor to the next word boundary (Ctrl+Right / word-right) */
static void moveCursorWordRight(LineEditor* this) {
   size_t pos = this->cursor;
   size_t len = this->len;
   /* skip non-whitespace */
   while (pos < len && !isspace((unsigned char)this->buffer[pos]))
      pos++;
   /* skip whitespace */
   while (pos < len && isspace((unsigned char)this->buffer[pos]))
      pos++;
   this->cursor = pos;
}

/* Delete the character before cursor (Backspace) */
static bool deleteCharBefore(LineEditor* this) {
   if (this->cursor == 0)
      return false;
   size_t pos = this->cursor - 1;
   memmove(this->buffer + pos, this->buffer + this->cursor, this->len - this->cursor + 1);
   this->len--;
   this->cursor = pos;
   return true;
}

/* Delete the character at cursor (Delete) */
static bool deleteCharAt(LineEditor* this) {
   if (this->cursor >= this->len)
      return false;
   memmove(this->buffer + this->cursor, this->buffer + this->cursor + 1, this->len - this->cursor);
   this->len--;
   return true;
}

/* Insert a printable character at cursor */
static bool insertChar(LineEditor* this, char ch) {
   if (this->len >= this->maxLen)
      return false;
   memmove(this->buffer + this->cursor + 1, this->buffer + this->cursor, this->len - this->cursor + 1);
   this->buffer[this->cursor] = ch;
   this->cursor++;
   this->len++;
   return true;
}

bool LineEditor_handleKey(LineEditor* this, int ch) {
   switch (ch) {
      case KEY_LEFT:
      case KEY_CTRL('B'):
         moveCursorLeft(this);
         return false;

      case KEY_RIGHT:
      case KEY_CTRL('F'):
         moveCursorRight(this);
         return false;

      case KEY_HOME:
      case KEY_CTRL('A'):
         this->cursor = 0;
         return false;

      case KEY_END:
      case KEY_CTRL('E'):
         this->cursor = this->len;
         return false;

      case KEY_SLEFT:      /* Shift+Left (ncurses stock) and Ctrl+Left (htop home grown) */
         moveCursorWordLeft(this);
         return false;

      case KEY_SRIGHT:     /* Shift+Right (ncurses stock) and Ctrl+Right (htop home grown) */
         moveCursorWordRight(this);
         return false;

      case KEY_DC: /* Delete */
         return deleteCharAt(this);

      case KEY_BACKSPACE:
      case 127: /* DEL / Backspace in some terminals */
         return deleteCharBefore(this);

      case KEY_CTRL('W'): {
         /* Delete word before cursor (like bash Ctrl-W) */
         size_t end = this->cursor;
         /* skip whitespace before cursor */
         while (this->cursor > 0 && isspace((unsigned char)this->buffer[this->cursor - 1]))
            this->cursor--;
         /* skip non-whitespace */
         while (this->cursor > 0 && !isspace((unsigned char)this->buffer[this->cursor - 1]))
            this->cursor--;
         if (this->cursor == end)
            return false;
         size_t deleted = end - this->cursor;
         memmove(this->buffer + this->cursor, this->buffer + end, this->len - end + 1);
         this->len -= deleted;
         return true;
      }

      case KEY_CTRL('K'):
         /* Delete from cursor to end of line */
         if (this->cursor >= this->len)
            return false;
         this->buffer[this->cursor] = '\0';
         this->len = this->cursor;
         return true;

      case KEY_CTRL('U'):
         /* Delete from start of line to cursor */
         if (this->cursor == 0)
            return false;
         memmove(this->buffer, this->buffer + this->cursor, this->len - this->cursor + 1);
         this->len -= this->cursor;
         this->cursor = 0;
         return true;

      default:
         if (ch > 0 && ch < 256 && isprint((unsigned char)ch)) {
            return insertChar(this, (char)ch);
         }
         return false;
   }
}

void LineEditor_updateScroll(LineEditor* this, int fieldWidth) {
   if (fieldWidth <= 0)
      return;
   size_t fw = (size_t)fieldWidth;
   /* Ensure cursor is visible */
   if (this->cursor < this->scroll) {
      this->scroll = this->cursor;
   } else if (this->cursor >= this->scroll + fw) {
      this->scroll = this->cursor - fw + 1;
   }
}

int LineEditor_draw(LineEditor* this, int startX, int fieldWidth, int attr) {
   if (attr == -1) {
      attrset(CRT_colors[FUNCTION_BAR]);
   } else {
      attrset(attr);
   }

   /* Display the visible portion of the buffer */
   const char* visibleStart = this->buffer + this->scroll;
   int visibleLen = (int)this->len - (int)this->scroll;
   if (visibleLen < 0)
      visibleLen = 0;
   if (visibleLen > fieldWidth)
      visibleLen = fieldWidth;

   mvaddnstr(LINES - 1, startX, visibleStart, visibleLen);

   /* Pad remaining field with spaces */
   for (int i = visibleLen; i < fieldWidth; i++) {
      mvaddch(LINES - 1, startX + i, ' ');
   }

   int cursorX = startX + (int)(this->cursor - this->scroll);
   return cursorX;
}

void LineEditor_click(LineEditor* this, int clickX, int fieldStartX) {
   int offset = clickX - fieldStartX;
   if (offset < 0)
      offset = 0;
   size_t newCursor = this->scroll + (size_t)offset;
   if (newCursor > this->len)
      newCursor = this->len;
   this->cursor = newCursor;
}
