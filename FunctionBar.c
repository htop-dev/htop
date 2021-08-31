/*
htop - FunctionBar.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FunctionBar.h"

#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static const char* const FunctionBar_FKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", NULL};

static const char* const FunctionBar_FLabels[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", NULL};

static int FunctionBar_FEvents[] = {KEY_F(1), KEY_F(2), KEY_F(3), KEY_F(4), KEY_F(5), KEY_F(6), KEY_F(7), KEY_F(8), KEY_F(9), KEY_F(10)};

static const char* const FunctionBar_EnterEscKeys[] = {"Enter", "Esc", NULL};
static const int FunctionBar_EnterEscEvents[] = {13, 27};

static int currentLen = 0;

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
      this->keys.keys = xCalloc(15, sizeof(char*));
      this->events = xCalloc(15, sizeof(int));
      int i = 0;
      while (i < 15 && functions[i]) {
         this->keys.keys[i] = xStrdup(keys[i]);
         this->events[i] = events[i];
         i++;
      }
      this->size = i;
   } else {
      this->staticData = true;
      this->keys.constKeys = FunctionBar_FKeys;
      this->events = FunctionBar_FEvents;
      this->size = ARRAYSIZE(FunctionBar_FEvents);
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
         free(this->keys.keys[i]);
      }
      free(this->keys.keys);
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

int FunctionBar_draw(const FunctionBar* this) {
   return FunctionBar_drawExtra(this, NULL, -1, false);
}

int FunctionBar_drawExtra(const FunctionBar* this, const char* buffer, int attr, bool setCursor) {
   int cursorX = 0;
   attrset(CRT_colors[FUNCTION_BAR]);
   mvhline(LINES - 1, 0, ' ', COLS);
   int x = 0;
   for (int i = 0; i < this->size; i++) {
      attrset(CRT_colors[FUNCTION_KEY]);
      mvaddstr(LINES - 1, x, this->keys.constKeys[i]);
      x += strlen(this->keys.constKeys[i]);
      attrset(CRT_colors[FUNCTION_BAR]);
      mvaddstr(LINES - 1, x, this->functions[i]);
      x += strlen(this->functions[i]);
   }

   if (buffer) {
      if (attr == -1) {
         attrset(CRT_colors[FUNCTION_BAR]);
      } else {
         attrset(attr);
      }
      mvaddstr(LINES - 1, x, buffer);
      x += strlen(buffer);
      cursorX = x;
   }

   attrset(CRT_colors[RESET_COLOR]);

   if (setCursor) {
      curs_set(1);
   } else {
      curs_set(0);
   }

   currentLen = x;

   return cursorX;
}

void FunctionBar_append(const char* buffer, int attr) {
   if (attr == -1) {
      attrset(CRT_colors[FUNCTION_BAR]);
   } else {
      attrset(attr);
   }
   mvaddstr(LINES - 1, currentLen + 1, buffer);
   attrset(CRT_colors[RESET_COLOR]);

   currentLen += strlen(buffer) + 1;
}

int FunctionBar_synthesizeEvent(const FunctionBar* this, int pos) {
   int x = 0;
   for (int i = 0; i < this->size; i++) {
      x += strlen(this->keys.constKeys[i]);
      x += strlen(this->functions[i]);
      if (pos < x) {
         return this->events[i];
      }
   }
   return ERR;
}
