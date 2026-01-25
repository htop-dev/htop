/*
htop - FunctionBar.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FunctionBar.h"
#include "Platform.h"

#include <assert.h>
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

static char warningBuf[256];
static bool warningActive;
static bool warningNeedsCleared;
static bool warningDismissOnKeypress;
static uint64_t warningExpiresAtMs;
static const uint32_t warningDefaultTimeoutMs = 1500;

FunctionBar* FunctionBar_newEnterEsc(const char* enter, const char* esc) {
   const char* functions[FUNCTIONBAR_MAXEVENTS + 1] = {enter, esc, NULL};
   return FunctionBar_new(functions, FunctionBar_EnterEscKeys, FunctionBar_EnterEscEvents);
}

FunctionBar* FunctionBar_new(const char* const* functions, const char* const* keys, const int* events) {
   FunctionBar* this = xCalloc(1, sizeof(FunctionBar));
   this->functions = xCalloc(FUNCTIONBAR_MAXEVENTS + 1, sizeof(char*));
   if (!functions) {
      functions = FunctionBar_FLabels;
   }
   for (size_t i = 0; i < FUNCTIONBAR_MAXEVENTS && functions[i]; i++) {
      this->functions[i] = xStrdup(functions[i]);
   }
   if (keys && events) {
      this->staticData = false;
      this->keys.keys = xCalloc(FUNCTIONBAR_MAXEVENTS, sizeof(char*));
      this->events = xCalloc(FUNCTIONBAR_MAXEVENTS, sizeof(int));
      size_t i = 0;
      while (i < FUNCTIONBAR_MAXEVENTS && functions[i]) {
         this->keys.keys[i] = xStrdup(keys[i]);
         this->events[i] = events[i];
         i++;
      }
      this->size = (uint32_t)i;
   } else {
      this->staticData = true;
      this->keys.constKeys = FunctionBar_FKeys;
      this->events = FunctionBar_FEvents;
      this->size = ARRAYSIZE(FunctionBar_FEvents);
   }
   assert(this->size <= FUNCTIONBAR_MAXEVENTS);
   return this;
}

void FunctionBar_delete(FunctionBar* this) {
   for (size_t i = 0; i < FUNCTIONBAR_MAXEVENTS && this->functions[i]; i++) {
      free(this->functions[i]);
   }
   free(this->functions);
   if (!this->staticData) {
      for (size_t i = 0; i < this->size; i++) {
         free(this->keys.keys[i]);
      }
      free(this->keys.keys);
      free(this->events);
   }
   free(this);
}

void FunctionBar_clearWarning(void) {
   warningActive = false;
   warningBuf[0] = '\0';
   warningDismissOnKeypress = false;
   warningExpiresAtMs = 0;
   warningNeedsCleared = true;
}

void FunctionBar_inputEvent(void) {
   if (warningDismissOnKeypress) {
      FunctionBar_clearWarning();
   }
}

void FunctionBar_setLabel(FunctionBar* this, int event, const char* text) {
   for (size_t i = 0; i < this->size; i++) {
      if (this->events[i] == event) {
         free(this->functions[i]);
         this->functions[i] = xStrdup(text);
         break;
      }
   }
}

void FunctionBar_setWarning(const char* msg, uint32_t timeoutMs, bool dismissOnKeypress) {
   if (msg == NULL || msg[0] == '\0') {
      FunctionBar_clearWarning();
      return;
   }
   snprintf(warningBuf, sizeof(warningBuf), "%s", msg);
   warningActive = true;
   warningDismissOnKeypress = dismissOnKeypress;
   uint64_t now = 0;
   Platform_gettime_monotonic(&now);
   if (timeoutMs == 0) {
      warningExpiresAtMs = now + warningDefaultTimeoutMs;
   }
   else {
      warningExpiresAtMs = now + timeoutMs;
   }
}

int FunctionBar_draw(const FunctionBar* this) {
   return FunctionBar_drawExtra(this, NULL, -1, false);
}

int FunctionBar_drawExtra(const FunctionBar* this, const char* buffer, int attr, bool setCursor) {
   int cursorX = 0;
   if (warningActive) {
      uint64_t now = 0;
      Platform_gettime_monotonic(&now);
      if (now >= warningExpiresAtMs) {
         FunctionBar_clearWarning();
      }
   }
   if (warningNeedsCleared && LINES > 1) {
      attrset(CRT_colors[RESET_COLOR]);
      mvhline(LINES - 2, 0, ' ', COLS);
      warningNeedsCleared = false;
   }
   if (warningActive && LINES > 1) {
      attrset(CRT_colors[FAILED_READ]);
      mvhline(LINES - 2, 0, ' ', COLS);
      mvaddstr(LINES - 2, 0, warningBuf);
   }
   attrset(CRT_colors[FUNCTION_BAR]);
   mvhline(LINES - 1, 0, ' ', COLS);
   int x = 0;
   for (size_t i = 0; i < this->size; i++) {
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
   for (size_t i = 0; i < this->size; i++) {
      x += strlen(this->keys.constKeys[i]);
      x += strlen(this->functions[i]);
      if (pos < x) {
         return this->events[i];
      }
   }
   return ERR;
}
