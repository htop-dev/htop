/*
htop - FunctionBar.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "FunctionBar.h"
#include "CRT.h"
#include "RichString.h"
#include "XAlloc.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>


static const char* const FunctionBar_FKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", NULL};

static const char* const FunctionBar_FLabels[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", NULL};

static int FunctionBar_FEvents[] = {KEY_F(1), KEY_F(2), KEY_F(3), KEY_F(4), KEY_F(5), KEY_F(6), KEY_F(7), KEY_F(8), KEY_F(9), KEY_F(10)};

static const char* const FunctionBar_EnterEscKeys[] = {"Enter", "Esc", NULL};
static const int FunctionBar_EnterEscEvents[] = {13, 27};

FunctionBar* FunctionBar_newEnterEsc(const char* enter, const char* esc) {
   const char* functions[] = {enter, esc, NULL};
   return FunctionBar_new(functions, FunctionBar_EnterEscKeys, FunctionBar_EnterEscEvents);
}

FunctionBar* FunctionBar_new(const char* const* functions, const char* const* keys, const int* events) {
   FunctionBar* this = xCalloc(1, sizeof(FunctionBar));
   this->functions = xCalloc(16, sizeof(char*));
   if (!functions) {
      functions = FunctionBar_FLabels;
   }
   for (int i = 0; i < 15 && functions[i]; i++) {
      this->functions[i] = xStrdup(functions[i]);
   }
   if (keys && events) {
      this->staticData = false;
      this->keys = xCalloc(15, sizeof(char*));
      this->events = xCalloc(15, sizeof(int));
      int i = 0;
      while (i < 15 && functions[i]) {
         this->keys[i] = xStrdup(keys[i]);
         this->events[i] = events[i];
         i++;
      }
      this->size = i;
   } else {
      this->staticData = true;
      this->keys = (char**) FunctionBar_FKeys;
      this->events = FunctionBar_FEvents;
      this->size = 10;
   }
   return this;
}

void FunctionBar_delete(FunctionBar* this) {
   for (int i = 0; i < 15 && this->functions[i]; i++) {
      free(this->functions[i]);
   }
   free(this->functions);
   if (!this->staticData) {
      for (int i = 0; i < this->size; i++) {
         free(this->keys[i]);
      }
      free(this->keys);
      free(this->events);
   }
   free(this);
}

void FunctionBar_setLabel(FunctionBar* this, int event, const char* text) {
   for (int i = 0; i < this->size; i++) {
      if (this->events[i] == event) {
         free(this->functions[i]);
         this->functions[i] = xStrdup(text);
         break;
      }
   }
}

void FunctionBar_draw(const FunctionBar* this, char* buffer) {
   FunctionBar_drawAttr(this, buffer, CRT_colors[FUNCTION_BAR]);
}

void FunctionBar_drawAttr(const FunctionBar* this, char* buffer, int attr) {
   attrset(CRT_colors[FUNCTION_BAR]);
   mvhline(LINES-1, 0, ' ', COLS);
   int x = 0;
   for (int i = 0; i < this->size; i++) {
      attrset(CRT_colors[FUNCTION_KEY]);
      mvaddstr(LINES-1, x, this->keys[i]);
      x += strlen(this->keys[i]);
      attrset(CRT_colors[FUNCTION_BAR]);
      mvaddstr(LINES-1, x, this->functions[i]);
      x += strlen(this->functions[i]);
   }
   if (buffer) {
      attrset(attr);
      mvaddstr(LINES-1, x, buffer);
      CRT_cursorX = x + strlen(buffer);
      curs_set(1);
   } else {
      curs_set(0);
   }
   attrset(CRT_colors[RESET_COLOR]);
}

int FunctionBar_synthesizeEvent(const FunctionBar* this, int pos) {
   int x = 0;
   for (int i = 0; i < this->size; i++) {
      x += strlen(this->keys[i]);
      x += strlen(this->functions[i]);
      if (pos < x) {
         return this->events[i];
      }
   }
   return ERR;
}
