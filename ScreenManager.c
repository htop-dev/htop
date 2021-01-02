/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreenManager.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Object.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "XUtils.h"


ScreenManager* ScreenManager_new(Header* header, const Settings* settings, const State* state, bool owner) {
   ScreenManager* this;
   this = xMalloc(sizeof(ScreenManager));
   this->x1 = 0;
   this->y1 = header->height;
   this->x2 = 0;
   this->y2 = -1;
   this->panels = Vector_new(Class(Panel), owner, DEFAULT_SIZE);
   this->panelCount = 0;
   this->header = header;
   this->settings = settings;
   this->state = state;
   this->owner = owner;
   this->allowFocusChange = true;
   return this;
}

void ScreenManager_delete(ScreenManager* this) {
   Vector_delete(this->panels);
   free(this);
}

inline int ScreenManager_size(ScreenManager* this) {
   return this->panelCount;
}

void ScreenManager_add(ScreenManager* this, Panel* item, int size) {
   int lastX = 0;
   if (this->panelCount > 0) {
      Panel* last = (Panel*) Vector_get(this->panels, this->panelCount - 1);
      lastX = last->x + last->w + 1;
   }
   int height = LINES - this->y1 + this->y2;
   if (size > 0) {
      Panel_resize(item, size, height);
   } else {
      Panel_resize(item, COLS - this->x1 + this->x2 - lastX, height);
   }
   Panel_move(item, lastX, this->y1);
   Vector_add(this->panels, item);
   item->needsRedraw = true;
   this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* this, int idx) {
   assert(this->panelCount > idx);
   Panel* panel = (Panel*) Vector_remove(this->panels, idx);
   this->panelCount--;
   return panel;
}

void ScreenManager_resize(ScreenManager* this, int x1, int y1, int x2, int y2) {
   this->x1 = x1;
   this->y1 = y1;
   this->x2 = x2;
   this->y2 = y2;
   int panels = this->panelCount;
   int lastX = 0;
   for (int i = 0; i < panels - 1; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_resize(panel, panel->w, LINES - y1 + y2);
      Panel_move(panel, lastX, y1);
      lastX = panel->x + panel->w + 1;
   }
   Panel* panel = (Panel*) Vector_get(this->panels, panels - 1);
   Panel_resize(panel, COLS - x1 + x2 - lastX, LINES - y1 + y2);
   Panel_move(panel, lastX, y1);
}

static void checkRecalculation(ScreenManager* this, double* oldTime, int* sortTimeout, bool* redraw, bool* rescan, bool* timedOut) {
   ProcessList* pl = this->header->pl;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   double newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);

   *timedOut = (newTime - *oldTime > this->settings->delay);
   *rescan |= *timedOut;

   if (newTime < *oldTime) {
      *rescan = true; // clock was adjusted?
   }

   if (*rescan) {
      *oldTime = newTime;
      ProcessList_scan(pl, this->state->pauseProcessUpdate);
      if (!this->state->pauseProcessUpdate && (*sortTimeout == 0 || this->settings->treeView)) {
         ProcessList_sort(pl);
         *sortTimeout = 1;
      }
      *redraw = true;
   }
   if (*redraw) {
      ProcessList_rebuildPanel(pl);
      Header_draw(this->header);
   }
   *rescan = false;
}

static void ScreenManager_drawPanels(ScreenManager* this, int focus, bool force_redraw) {
   const int nPanels = this->panelCount;
   for (int i = 0; i < nPanels; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_draw(panel, force_redraw, i == focus, !((panel == this->state->panel) && this->state->hideProcessSelection));
      mvvline(panel->y, panel->x + panel->w, ' ', panel->h);
   }
}

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey) {
   bool quit = false;
   int focus = 0;

   Panel* panelFocus = (Panel*) Vector_get(this->panels, focus);

   double oldTime = 0.0;

   int ch = ERR;
   int closeTimeout = 0;

   bool timedOut = true;
   bool redraw = true;
   bool force_redraw = true;
   bool rescan = false;
   int sortTimeout = 0;
   int resetSortTimeout = 5;

   while (!quit) {
      if (this->header) {
         checkRecalculation(this, &oldTime, &sortTimeout, &redraw, &rescan, &timedOut);
      }

      if (redraw || force_redraw) {
         ScreenManager_drawPanels(this, focus, force_redraw);
         force_redraw = false;
      }

      int prevCh = ch;
      set_escdelay(25);
      ch = getch();

      HandlerResult result = IGNORED;
      if (ch == KEY_MOUSE && this->settings->enableMouse) {
         ch = ERR;
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.bstate & BUTTON1_RELEASED) {
               if (mevent.y == LINES - 1) {
                  ch = FunctionBar_synthesizeEvent(panelFocus->currentBar, mevent.x);
               } else {
                  for (int i = 0; i < this->panelCount; i++) {
                     Panel* panel = (Panel*) Vector_get(this->panels, i);
                     if (mevent.x >= panel->x && mevent.x <= panel->x + panel->w) {
                        if (mevent.y == panel->y) {
                           ch = EVENT_HEADER_CLICK(mevent.x - panel->x);
                           break;
                        } else if (mevent.y > panel->y && mevent.y <= panel->y + panel->h) {
                           ch = KEY_MOUSE;
                           if (panel == panelFocus || this->allowFocusChange) {
                              focus = i;
                              panelFocus = panel;
                              Object* oldSelection = Panel_getSelected(panel);
                              Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                              if (Panel_getSelected(panel) == oldSelection) {
                                 ch = KEY_RECLICK;
                              }
                           }
                           break;
                        }
                     }
                  }
               }
            #if NCURSES_MOUSE_VERSION > 1
            } else if (mevent.bstate & BUTTON4_PRESSED) {
               ch = KEY_WHEELUP;
            } else if (mevent.bstate & BUTTON5_PRESSED) {
               ch = KEY_WHEELDOWN;
            #endif
            }
         }
      }
      if (ch == ERR) {
         sortTimeout--;
         if (prevCh == ch && !timedOut) {
            closeTimeout++;
            if (closeTimeout == 100) {
               break;
            }
         } else {
            closeTimeout = 0;
         }
         redraw = false;
         continue;
      }
      switch (ch) {
         case KEY_ALT('H'): ch = KEY_LEFT; break;
         case KEY_ALT('J'): ch = KEY_DOWN; break;
         case KEY_ALT('K'): ch = KEY_UP; break;
         case KEY_ALT('L'): ch = KEY_RIGHT; break;
      }
      redraw = true;
      if (Panel_eventHandlerFn(panelFocus)) {
         result = Panel_eventHandler(panelFocus, ch);
      }
      if (result & SYNTH_KEY) {
         ch = result >> 16;
      }
      if (result & REFRESH) {
         sortTimeout = 0;
      }
      if (result & REDRAW) {
         force_redraw = true;
      }
      if (result & RESCAN) {
         rescan = true;
         sortTimeout = 0;
      }
      if (result & HANDLED) {
         continue;
      } else if (result & BREAK_LOOP) {
         quit = true;
         continue;
      }

      switch (ch) {
      case KEY_RESIZE:
      {
         ScreenManager_resize(this, this->x1, this->y1, this->x2, this->y2);
         continue;
      }
      case KEY_LEFT:
      case KEY_CTRL('B'):
         if (this->panelCount < 2) {
            goto defaultHandler;
         }

         if (!this->allowFocusChange) {
            break;
         }

tryLeft:
         if (focus > 0) {
            focus--;
         }

         panelFocus = (Panel*) Vector_get(this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus > 0) {
            goto tryLeft;
         }

         break;
      case KEY_RIGHT:
      case KEY_CTRL('F'):
      case 9:
         if (this->panelCount < 2) {
            goto defaultHandler;
         }
         if (!this->allowFocusChange) {
            break;
         }

tryRight:
         if (focus < this->panelCount - 1) {
            focus++;
         }

         panelFocus = (Panel*) Vector_get(this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus < this->panelCount - 1) {
            goto tryRight;
         }

         break;
      case 27:
      case 'q':
      case KEY_F(10):
         quit = true;
         continue;
      default:
defaultHandler:
         sortTimeout = resetSortTimeout;
         Panel_onKey(panelFocus, ch);
         break;
      }
   }

   if (lastFocus) {
      *lastFocus = panelFocus;
   }

   if (lastKey) {
      *lastKey = ch;
   }
}
