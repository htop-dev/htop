#ifndef HEADER_FunctionBar
#define HEADER_FunctionBar
/*
htop - FunctionBar.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

typedef struct FunctionBar_ {
   int size;
   char** functions;
   char** keys;
   int* events;
   bool staticData;
} FunctionBar;

FunctionBar* FunctionBar_newEnterEsc(const char* enter, const char* esc);

FunctionBar* FunctionBar_new(const char* const* functions, const char* const* keys, const int* events);

void FunctionBar_delete(FunctionBar* this);

void FunctionBar_setLabel(FunctionBar* this, int event, const char* text);

void FunctionBar_draw(const FunctionBar* this, char* buffer);

void FunctionBar_drawAttr(const FunctionBar* this, char* buffer, int attr);

int FunctionBar_synthesizeEvent(const FunctionBar* this, int pos);

#endif
