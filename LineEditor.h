#ifndef HEADER_LineEditor
#define HEADER_LineEditor
/*
htop - LineEditor.h
(C) 2004-2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>


#define LINEEDITOR_MAX 128

typedef struct LineEditor_ {
   char buffer[LINEEDITOR_MAX + 1];
   size_t len;        /* current text length */
   size_t cursor;     /* cursor byte position (0..len) */
   size_t scroll;     /* display scroll offset */
   size_t maxLen;     /* maximum allowed input length (0 = use LINEEDITOR_MAX) */
} LineEditor;

/* Initialize a LineEditor with default max length */
void LineEditor_init(LineEditor* this);

/* Initialize a LineEditor with a custom maximum length */
void LineEditor_initWithMax(LineEditor* this, size_t maxLen);

/* Reset the editor to empty content */
void LineEditor_reset(LineEditor* this);

/* Set the buffer content and move cursor to the end */
void LineEditor_setText(LineEditor* this, const char* text);

/* Get a pointer to the current buffer content */
static inline char* LineEditor_getText(LineEditor* this) {
   return this->buffer;
}

/* Get current cursor position */
static inline size_t LineEditor_getCursor(LineEditor* this) {
   return this->cursor;
}

/* Process a keypress; returns true if the text content changed */
bool LineEditor_handleKey(LineEditor* this, int ch);

/* Update scroll offset so cursor is visible in a field of given display width.
   Should be called before drawing whenever the cursor may have moved. */
void LineEditor_updateScroll(LineEditor* this, int fieldWidth);

/* Draw the visible portion of the buffer starting at screen column startX,
   for a field of fieldWidth columns, using the given ncurses attribute.
   Returns the screen X position where the cursor should be placed. */
int LineEditor_draw(LineEditor* this, int startX, int fieldWidth, int attr);

/* Handle a mouse click at screen column clickX, given that the field starts
   at screen column fieldStartX. Moves the cursor to the appropriate position. */
void LineEditor_click(LineEditor* this, int clickX, int fieldStartX);

#endif
