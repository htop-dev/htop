/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ScreenManager.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "Settings.h"
#include "Table.h"
#include "XUtils.h"


ScreenManager* ScreenManager_new(Header* header, Machine* host, State* state, bool owner) {
   ScreenManager* this;
   this = xMalloc(sizeof(ScreenManager));
   this->x1 = 0;
   this->y1 = 0;
   this->x2 = 0;
   this->y2 = -1;
   this->panels = Vector_new(Class(Panel), owner, DEFAULT_SIZE);
   this->panelCount = 0;
   this->header = header;
   this->host = host;
   this->state = state;
   this->allowFocusChange = true;
   return this;
}

void ScreenManager_delete(ScreenManager* this) {
   Vector_delete(this->panels);
   free(this);
}

inline int ScreenManager_size(const ScreenManager* this) {
   return this->panelCount;
}

void ScreenManager_add(ScreenManager* this, Panel* item, int size) {
   ScreenManager_insert(this, item, size, Vector_size(this->panels));
}

static int header_height(const ScreenManager* this) {
   if (this->state->hideMeters)
      return 0;

   if (this->header)
      return this->header->height;

   return 0;
}

void ScreenManager_insert(ScreenManager* this, Panel* item, int size, int idx) {
   int lastX = 0;
   if (idx > 0) {
      const Panel* last = (const Panel*) Vector_get(this->panels, idx - 1);
      lastX = last->x + last->w + 1;
   }
   int height = LINES - this->y1 - header_height(this) + this->y2;
   if (size <= 0) {
      size = COLS - this->x1 + this->x2 - lastX;
   }
   Panel_resize(item, size, height);
   Panel_move(item, lastX, this->y1 + header_height(this));
   if (idx < this->panelCount) {
      for (int i =  idx + 1; i <= this->panelCount; i++) {
         Panel* p = (Panel*) Vector_get(this->panels, i);
         Panel_move(p, p->x + size, p->y);
      }
   }
   Vector_insert(this->panels, idx, item);
   item->needsRedraw = true;
   this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* this, int idx) {
   assert(this->panelCount > idx);
   int w = ((Panel*) Vector_get(this->panels, idx))->w;
   Panel* panel = (Panel*) Vector_remove(this->panels, idx);
   this->panelCount--;
   if (idx < this->panelCount) {
      for (int i = idx; i < this->panelCount; i++) {
         Panel* p = (Panel*) Vector_get(this->panels, i);
         Panel_move(p, p->x - w, p->y);
      }
   }
   return panel;
}

void ScreenManager_resize(ScreenManager* this) {
   int y1_header = this->y1 + header_height(this);
   int panels = this->panelCount;
   int lastX = 0;
   for (int i = 0; i < panels - 1; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_resize(panel, panel->w, LINES - y1_header + this->y2);
      Panel_move(panel, lastX, y1_header);
      lastX = panel->x + panel->w + 1;
   }
   Panel* panel = (Panel*) Vector_get(this->panels, panels - 1);
   Panel_resize(panel, COLS - this->x1 + this->x2 - lastX, LINES - y1_header + this->y2);
   Panel_move(panel, lastX, y1_header);
}

static void checkRecalculation(ScreenManager* this, double* oldTime, int* sortTimeout, bool* redraw, bool* rescan, bool* timedOut, bool* force_redraw) {
   Machine* host = this->host;

   Platform_gettime_realtime(&host->realtime, &host->realtimeMs);
   double newTime = ((double)host->realtime.tv_sec * 10) + ((double)host->realtime.tv_usec / 100000);

   *timedOut = (newTime - *oldTime > host->settings->delay);
   *rescan |= *timedOut;

   if (newTime < *oldTime) {
      *rescan = true; // clock was adjusted?
   }

   if (*rescan) {
      *oldTime = newTime;
      int oldUidDigits = Process_uidDigits;
      if (!this->state->pauseUpdate && (*sortTimeout == 0 || host->settings->ss->treeView)) {
         host->activeTable->needsSort = true;
         *sortTimeout = 1;
      }
      // sample current values for system metrics and processes if not paused
      Machine_scan(host);
      if (!this->state->pauseUpdate)
         Machine_scanTables(host);

      // always update header, especially to avoid gaps in graph meters
      Header_updateData(this->header);
      // force redraw if the number of UID digits was changed
      if (Process_uidDigits != oldUidDigits) {
         *force_redraw = true;
      }
      *redraw = true;
   }
   if (*redraw) {
      Table_rebuildPanel(host->activeTable);
      if (!this->state->hideMeters)
         Header_draw(this->header);
   }
   *rescan = false;
}

static inline bool drawTab(const int* y, int* x, int l, const char* name, bool cur) {
   attrset(CRT_colors[cur ? SCREENS_CUR_BORDER : SCREENS_OTH_BORDER]);
   mvaddch(*y, *x, '[');
   (*x)++;
   if (*x >= l)
      return false;
   int nameLen = strlen(name);
   int n = MINIMUM(l - *x, nameLen);
   attrset(CRT_colors[cur ? SCREENS_CUR_TEXT : SCREENS_OTH_TEXT]);
   mvaddnstr(*y, *x, name, n);
   *x += n;
   if (*x >= l)
      return false;
   attrset(CRT_colors[cur ? SCREENS_CUR_BORDER : SCREENS_OTH_BORDER]);
   mvaddch(*y, *x, ']');
   *x += 2;
   if (*x >= l)
      return false;
   return true;
}

static void ScreenManager_drawScreenTabs(ScreenManager* this) {
   Settings* settings = this->host->settings;
   ScreenSettings** screens = settings->screens;
   int cur = settings->ssIndex;
   int l = COLS;
   Panel* panel = (Panel*) Vector_get(this->panels, 0);
   int y = panel->y - 1;
   int x = 2;

   if (this->name) {
      drawTab(&y, &x, l, this->name, true);
      return;
   }

   for (int s = 0; screens[s]; s++) {
      bool ok = drawTab(&y, &x, l, screens[s]->heading, s == cur);
      if (!ok) {
         break;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
}

static void ScreenManager_drawPanels(ScreenManager* this, int focus, bool force_redraw) {
   Settings* settings = this->host->settings;
   if (settings->screenTabs) {
      ScreenManager_drawScreenTabs(this);
   }
   const int nPanels = this->panelCount;
   for (int i = 0; i < nPanels; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_draw(panel,
                 force_redraw,
                 i == focus,
                 panel != (Panel*)this->state->mainPanel || !this->state->hideSelection,
                 State_hideFunctionBar(this->state));
      mvvline(panel->y, panel->x + panel->w, ' ', panel->h + (State_hideFunctionBar(this->state) ? 1 : 0));
   }
}

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey, const char* name) {
   bool quit = false;
   int focus = 0;

   Panel* panelFocus = (Panel*) Vector_get(this->panels, focus);
   Settings* settings = this->host->settings;

   double oldTime = 0.0;

   int ch = ERR;
   int closeTimeout = 0;

   bool timedOut = true;
   bool redraw = true;
   bool force_redraw = true;
   bool rescan = false;
   int sortTimeout = 0;
   int resetSortTimeout = 5;

   this->name = name;

   while (!quit) {
      if (this->header) {
         checkRecalculation(this, &oldTime, &sortTimeout, &redraw, &rescan, &timedOut, &force_redraw);
      }

      if (redraw || force_redraw) {
         ScreenManager_drawPanels(this, focus, force_redraw);
         force_redraw = false;
         if (this->host->iterationsRemaining != -1) {
            if (!--this->host->iterationsRemaining) {
               quit = true;
               continue;
            }
         }
      }

      int prevCh = ch;
      ch = Panel_getCh(panelFocus);

      HandlerResult result = IGNORED;
#ifdef HAVE_GETMOUSE
      if (ch == KEY_MOUSE && settings->enableMouse) {
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
                        } else if (settings->screenTabs && mevent.y == panel->y - 1) {
                           ch = EVENT_SCREEN_TAB_CLICK(mevent.x);
                           break;
                        } else if (mevent.y > panel->y && mevent.y <= panel->y + panel->h) {
                           ch = KEY_MOUSE;
                           if (panel == panelFocus || this->allowFocusChange) {
                              focus = i;
                              panelFocus = panel;
                              const Object* oldSelection = Panel_getSelected(panel);
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
#endif
      if (ch == ERR) {
         if (sortTimeout > 0)
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
      if (result & RESIZE) {
         ScreenManager_resize(this);
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
         ScreenManager_resize(this);
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
      case '#':
         this->state->hideMeters = !this->state->hideMeters;
         ScreenManager_resize(this);
         force_redraw = true;
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
