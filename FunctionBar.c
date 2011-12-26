/*
htop - FunctionBar.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "FunctionBar.h"

#include "CRT.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <curses.h>

/*{
#include "Object.h"

typedef struct FunctionBar_ {
   Object super;
   int size;
   char** functions;
   char** keys;
   int* events;
   bool staticData;
} FunctionBar;

}*/

#ifdef DEBUG
char* FUNCTIONBAR_CLASS = "FunctionBar";
#else
#define FUNCTIONBAR_CLASS NULL
#endif

static const char* FunctionBar_FKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", NULL};

static const char* FunctionBar_FLabels[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", NULL};

static int FunctionBar_FEvents[] = {KEY_F(1), KEY_F(2), KEY_F(3), KEY_F(4), KEY_F(5), KEY_F(6), KEY_F(7), KEY_F(8), KEY_F(9), KEY_F(10)};

FunctionBar* FunctionBar_new(const char** functions, const char** keys, int* events) {
   FunctionBar* this = malloc(sizeof(FunctionBar));
   Object_setClass(this, FUNCTIONBAR_CLASS);
   ((Object*) this)->delete = FunctionBar_delete;
   this->functions = (char**) functions;
   if (keys && events) {
      this->staticData = false; 
      this->functions = malloc(sizeof(char*) * 15);
      this->keys = malloc(sizeof(char*) * 15);
      this->events = malloc(sizeof(int) * 15);
      int i = 0;
      while (i < 15 && functions[i]) {
         this->functions[i] = strdup(functions[i]);
         this->keys[i] = strdup(keys[i]);
         this->events[i] = events[i];
         i++;
      }
      this->size = i;
   } else {
      this->staticData = true;
      this->functions = (char**)( functions ? functions : FunctionBar_FLabels );
      this->keys = (char**) FunctionBar_FKeys;
      this->events = FunctionBar_FEvents;
      this->size = 10;
   }
   return this;
}

void FunctionBar_delete(Object* cast) {
   FunctionBar* this = (FunctionBar*) cast;
   if (!this->staticData) {
      for (int i = 0; i < this->size; i++) {
         free(this->functions[i]);
         free(this->keys[i]);
      }
      free(this->functions);
      free(this->keys);
      free(this->events);
   }
   free(this);
}

void FunctionBar_setLabel(FunctionBar* this, int event, const char* text) {
   assert(!this->staticData);
   for (int i = 0; i < this->size; i++) {
      if (this->events[i] == event) {
         free(this->functions[i]);
         this->functions[i] = strdup(text);
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
