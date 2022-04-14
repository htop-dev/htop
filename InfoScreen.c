#include "config.h" // IWYU pragma: keep

#include "InfoScreen.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static const char* const InfoScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* const InfoScreenKeys[] = {"F3", "F4", "F5", "Esc"};

static const int InfoScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

InfoScreen* InfoScreen_init(InfoScreen* this, const Process* process, FunctionBar* bar, int height, const char* panelHeader) {
   this->process = process;
   if (!bar) {
      bar = FunctionBar_new(InfoScreenFunctions, InfoScreenKeys, InfoScreenEvents);
   }
   this->display = Panel_new(0, 1, COLS, height, Class(ListItem), false, bar);
   this->inc = IncSet_new(bar);
   this->lines = Vector_new(Vector_type(this->display->items), true, DEFAULT_SIZE);
   Panel_setHeader(this->display, panelHeader);
   return this;
}

InfoScreen* InfoScreen_done(InfoScreen* this) {
   Panel_delete((Object*)this->display);
   IncSet_delete(this->inc);
   Vector_delete(this->lines);
   return this;
}

void InfoScreen_drawTitled(InfoScreen* this, const char* fmt, ...) {
   va_list ap;
   va_start(ap, fmt);

   char title[COLS + 1];
   int len = vsnprintf(title, sizeof(title), fmt, ap);
   va_end(ap);

   if (len > COLS) {
      memset(&title[COLS - 3], '.', 3);
   }

   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvaddstr(0, 0, title);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true, true, true, false);

   IncSet_drawBar(this->inc, CRT_colors[FUNCTION_BAR]);
}

void InfoScreen_addLine(InfoScreen* this, const char* line) {
   Vector_add(this->lines, (Object*) ListItem_new(line, 0));
   const char* incFilter = IncSet_filter(this->inc);
   if (!incFilter || String_contains_i(line, incFilter, true)) {
      Panel_add(this->display, Vector_get(this->lines, Vector_size(this->lines) - 1));
   }
}

void InfoScreen_appendLine(InfoScreen* this, const char* line) {
   ListItem* last = (ListItem*)Vector_get(this->lines, Vector_size(this->lines) - 1);
   ListItem_append(last, line);
   const char* incFilter = IncSet_filter(this->inc);
   if (incFilter && Panel_get(this->display, Panel_size(this->display) - 1) != (Object*)last && String_contains_i(line, incFilter, true)) {
      Panel_add(this->display, (Object*)last);
   }
}

void InfoScreen_run(InfoScreen* this) {
   Panel* panel = this->display;

   if (As_InfoScreen(this)->scan)
      InfoScreen_scan(this);

   InfoScreen_draw(this);

   bool looping = true;
   while (looping) {

      Panel_draw(panel, false, true, true, false);
      IncSet_drawBar(this->inc, CRT_colors[FUNCTION_BAR]);

      int ch = Panel_getCh(panel);

      if (ch == ERR) {
         if (As_InfoScreen(this)->onErr) {
            InfoScreen_onErr(this);
            continue;
         }
      }

#ifdef HAVE_GETMOUSE
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.bstate & BUTTON1_RELEASED) {
               if (mevent.y >= panel->y && mevent.y < LINES - 1) {
                  Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                  ch = 0;
               } else if (mevent.y == LINES - 1) {
                  ch = IncSet_synthesizeEvent(this->inc, mevent.x);
               }
            }
            #if NCURSES_MOUSE_VERSION > 1
            else if (mevent.bstate & BUTTON4_PRESSED) {
               ch = KEY_WHEELUP;
            } else if (mevent.bstate & BUTTON5_PRESSED) {
               ch = KEY_WHEELDOWN;
            }
            #endif
         }
      }
#endif

      if (this->inc->active) {
         IncSet_handleKey(this->inc, ch, panel, IncSet_getListItemValue, this->lines);
         continue;
      }

      switch (ch) {
      case ERR:
         continue;
      case KEY_F(3):
      case '/':
         IncSet_activate(this->inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(this->inc, INC_FILTER, panel);
         break;
      case KEY_F(5):
         clear();
         if (As_InfoScreen(this)->scan) {
            Vector_prune(this->lines);
            InfoScreen_scan(this);
         }

         InfoScreen_draw(this);
         break;
      case '\014': // Ctrl+L
         clear();
         InfoScreen_draw(this);
         break;
      case 27:
      case 'q':
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES - 2);
         if (As_InfoScreen(this)->scan) {
            Vector_prune(this->lines);
            InfoScreen_scan(this);
         }

         InfoScreen_draw(this);
         break;
      default:
         if (As_InfoScreen(this)->onKey && InfoScreen_onKey(this, ch)) {
            continue;
         }
         Panel_onKey(panel, ch);
      }
   }
}
