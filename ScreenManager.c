/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ScreenManager.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
   this->panels = Vector_new(Class(Panel), owner, VECTOR_DEFAULT_SIZE);
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
   if ((size_t)idx < this->panelCount) {
      for (int i =  idx + 1; (size_t)i <= this->panelCount; i++) {
         Panel* p = (Panel*) Vector_get(this->panels, i);
         Panel_move(p, p->x + size, p->y);
      }
   }
   Vector_insert(this->panels, idx, item);
   item->needsRedraw = true;
   this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* this, int idx) {
   assert((size_t)idx < this->panelCount);
   int w = ((Panel*) Vector_get(this->panels, idx))->w;
   Panel* panel = (Panel*) Vector_remove(this->panels, idx);
   this->panelCount--;
   if ((size_t)idx < this->panelCount) {
      for (size_t i = idx; i < this->panelCount; i++) {
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
   double newTime = ((double)host->realtime.tv_sec * 10) + ((double)host->realtime.tv_nsec / 100000000L);

   *timedOut = (newTime - *oldTime > host->settings->delay);
   *rescan |= *timedOut;

   if (newTime < *oldTime) {
      *rescan = true; // clock was adjusted?
   }

   if (*rescan) {
      *oldTime = newTime;

      if (!this->state->pauseUpdate && (*sortTimeout == 0 || host->settings->ss->treeView)) {
         host->activeTable->needsSort = true;
         *sortTimeout = 1;
      }

      int oldUidDigits = Process_uidDigits;
      int oldPidDigits = Process_pidDigits;

      // sample current values for system metrics and processes if not paused
      Machine_scan(host);
      if (!this->state->pauseUpdate)
         Machine_scanTables(host);
      this->state->failedUpdate = Platform_getFailedState();

      // always update header, especially to avoid gaps in graph meters
      if (Header_updateData(this->header)) {
         ScreenManager_resize(this);
         *force_redraw = true;
      }

      // force redraw if the number of UID/PID digits changed
      if (Process_uidDigits != oldUidDigits || Process_pidDigits != oldPidDigits)
         *force_redraw = true;

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
   assert(*x >= 0);
   assert(*x < l);

   attrset(CRT_colors[cur ? SCREENS_CUR_BORDER : SCREENS_OTH_BORDER]);
   mvaddch(*y, *x, '[');
   (*x)++;
   if (*x >= l)
      return false;
   int nameWidth = (int)strnlen(name, l - *x);
   attrset(CRT_colors[cur ? SCREENS_CUR_TEXT : SCREENS_OTH_TEXT]);
   mvaddnstr(*y, *x, name, nameWidth);
   *x += nameWidth;
   if (*x >= l)
      return false;
   attrset(CRT_colors[cur ? SCREENS_CUR_BORDER : SCREENS_OTH_BORDER]);
   mvaddch(*y, *x, ']');
   if (*x >= l - (1 + SCREEN_TAB_COLUMN_GAP)) {
      *x = l;
      return false;
   }
   *x += 1 + SCREEN_TAB_COLUMN_GAP;
   return true;
}

static void ScreenManager_drawScreenTabs(ScreenManager* this) {
   Settings* settings = this->host->settings;
   ScreenSettings** screens = settings->screens;
   int cur = settings->ssIndex;
   int l = COLS;
   Panel* panel = (Panel*) Vector_get(this->panels, 0);
   int y = panel->y - 1;
   int x = SCREEN_TAB_MARGIN_LEFT;

   if (x >= l)
      goto end;

   if (this->name) {
      drawTab(&y, &x, l, this->name, true);
      goto end;
   }

   for (int s = 0; screens[s]; s++) {
      bool ok = drawTab(&y, &x, l, screens[s]->heading, s == cur);
      if (!ok) {
         break;
      }
   }

end:
   attrset(CRT_colors[RESET_COLOR]);
}

static void ScreenManager_drawPanels(ScreenManager* this, size_t focus, bool force_redraw) {
   Settings* settings = this->host->settings;
   if (settings->screenTabs) {
      ScreenManager_drawScreenTabs(this);
   }
   const size_t nPanels = this->panelCount;
   for (size_t i = 0; i < nPanels; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_draw(panel,
                 force_redraw,
                 i == focus,
                 panel != (Panel*)this->state->mainPanel || !this->state->hideSelection,
                 State_hideFunctionBar(this->state));
      mvvline(panel->y, panel->x + panel->w, ' ', panel->h + (State_hideFunctionBar(this->state) ? 1 : 0));
   }
}

/* Every redraw is followed by a cooldown of this length. Updates applied
 * between scans (see Machine.wakeupFd) during it are drawn once it ends,
 * so a burst of events, e.g. a parallel build starting and ending
 * thousands of processes, does not keep htop redrawing in a tight loop,
 * while the latest state is still shown ahead of the next scan. The
 * updates themselves are applied as soon as they arrive. */
#define SCREENMANAGER_REDRAW_COOLDOWN_MS 100

static uint64_t ScreenManager_nowMs(void) {
   struct timespec now;
   uint64_t nowMs;
   Platform_gettime_realtime(&now, &nowMs);
   return nowMs;
}

/*
 * Waits for the next key like Panel_getCh() does, that is until the next
 * periodic scan is due. If the platform provides a wakeup file descriptor,
 * it is watched as well: once it turns readable, the wait ends with ERR and
 * *wakeup set. If redrawAtMs is non-zero, the wait ends no later than then
 * with ERR and *redrawDue set. oldTime is the time of the last scan in
 * tenths of seconds, as kept by checkRecalculation().
 */
static int ScreenManager_getCh(const ScreenManager* this, Panel* panel, double oldTime, uint64_t redrawAtMs, bool* wakeup, bool* redrawDue) {
   const Machine* host = this->host;
   const int wakeupFd = host->wakeupFd;

   *wakeup = false;
   *redrawDue = false;

   if (!this->header || wakeupFd < 0 || this->state->pauseUpdate)
      return Panel_getCh(panel);

   /* Rounded up, so the scan is surely due once the wait ends */
   const uint64_t lastScanMs = (uint64_t)(oldTime * 100) + 1;
   const uint64_t dueMs = lastScanMs + (uint64_t)host->settings->delay * 100;

   /* Keys already buffered by curses do not make stdin readable,
    * so curses is always asked first, without blocking */
   CRT_disableDelay();

   int ch;
   for (;;) {
      ch = Panel_getCh(panel);
      if (ch != ERR)
         break;

      const uint64_t nowMs = ScreenManager_nowMs();

      if (redrawAtMs && nowMs >= redrawAtMs) {
         *redrawDue = true;
         break;
      }

      /* Also stop if the clock went backwards; checkRecalculation() rescans then */
      if (nowMs >= dueMs || nowMs < lastScanMs - 1)
         break;

      uint64_t timeoutMs = dueMs - nowMs;
      if (redrawAtMs)
         timeoutMs = MINIMUM(timeoutMs, redrawAtMs - nowMs);

      struct pollfd fds[2] = {
         { .fd = STDIN_FILENO, .events = POLLIN },
         { .fd = wakeupFd, .events = POLLIN },
      };
      int r = poll(fds, ARRAYSIZE(fds), (int)timeoutMs);
      if (r < 0) {
         /* e.g. SIGWINCH: curses reports KEY_RESIZE on the next read */
         if (errno == EINTR)
            continue;

         break;
      }

      /* A gone terminal: report ERR right away, like curses does */
      if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
         break;

      if (fds[1].revents & POLLIN) {
         *wakeup = true;
         break;
      }

      /* A broken wakeup source must not turn the wait into a busy loop */
      if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
         CRT_enableDelay();
         return Panel_getCh(panel);
      }
   }

   CRT_enableDelay();
   return ch;
}

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey, const char* name) {
   bool quit = false;
   size_t focus = 0;

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

   /* Updates applied between scans that are not on screen yet, the end of
    * the cooldown after the last redraw, and whether the current redraw
    * shows such updates (not counted towards --max-iterations) */
   bool updatesPending = false;
   uint64_t cooldownEndMs = 0;
   bool updateRedraw = false;

   this->name = name;

   while (!quit) {
      if (this->header) {
         const uint64_t lastScanMs = this->host->monotonicMs;
         checkRecalculation(this, &oldTime, &sortTimeout, &redraw, &rescan, &timedOut, &force_redraw);

         /* A scan shows everything applied before it */
         if (this->host->monotonicMs != lastScanMs && !this->state->pauseUpdate)
            updatesPending = false;
      }

      if (redraw || force_redraw) {
         ScreenManager_drawPanels(this, focus, force_redraw);
         force_redraw = false;
         cooldownEndMs = ScreenManager_nowMs() + SCREENMANAGER_REDRAW_COOLDOWN_MS;
         if (updateRedraw) {
            updateRedraw = false;
         } else if (this->host->iterationsRemaining != -1) {
            if (!--this->host->iterationsRemaining) {
               quit = true;
               continue;
            }
         }
      }

      int prevCh = ch;
      bool wakeup;
      bool updateRedrawDue;
      ch = ScreenManager_getCh(this, panelFocus, oldTime, updatesPending ? cooldownEndMs : 0, &wakeup, &updateRedrawDue);

      if (wakeup) {
         /* Updates between scans (e.g. processes that started or ended) are
          * applied at once; they show up when the cooldown has ended. Other
          * redraws until then need not show them (e.g. a tree view that is
          * not rebuilt), so they stay pending. */
         if (Machine_handleWakeup(this->host))
            updatesPending = true;
         closeTimeout = 0;
         redraw = false;
         continue;
      }

      if (updateRedrawDue) {
         Table* table = this->host->activeTable;
         if (!this->state->pauseUpdate && (sortTimeout == 0 || settings->ss->treeView))
            table->needsSort = true;
         updatesPending = false;
         redraw = true;
         updateRedraw = true;
         closeTimeout = 0;
         continue;
      }

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
                  /* When the panel is in cursor-input mode and the click landed past
                     the function keys (in the text input area), signal a bar click. */
                  if (ch == ERR && panelFocus->cursorOn) {
                     panelFocus->lastMouseBarClickX = mevent.x;
                     ch = KEY_MOUSE_BAR_CLICK;
                  }
} else if (!this->state->hideMeters && this->header &&
                      mevent.x >= this->x1 && mevent.x < COLS + this->x2 &&
                      mevent.y >= this->y1 && mevent.y < this->y1 + header_height(this) - (settings->screenTabs ? 1 : 0)) {
                   /* Click in the meters/header area (excluding the screen-tab row) */
                  int meterReaction = Header_click(this->header, mevent.x - this->x1, mevent.y - this->y1);
                  if (meterReaction & HTOP_RECALCULATE) {
                     rescan = true;
                     sortTimeout = 0;
                  }
                  if (meterReaction & HTOP_SAVE_SETTINGS) {
                     this->host->settings->changed = true;
                  }
                  if (meterReaction & HTOP_REFRESH) {
                     sortTimeout = 0;
                  }
                  if (meterReaction) {
                     force_redraw = true;
                  }
               } else {
                  for (size_t i = 0; i < this->panelCount; i++) {
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
                              bool changingFocus = (i != focus);
                              if (changingFocus && Panel_eventHandlerFn(panelFocus)) {
                                 Panel_eventHandler(panelFocus, EVENT_PANEL_LOST_FOCUS);
                              }
                              focus = i;
                              panelFocus = panel;
                              const Object* oldSelection = Panel_getSelected(panel);
                              Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                              if (!changingFocus && Panel_getSelected(panel) == oldSelection) {
                                 ch = KEY_RECLICK;
                              }
                           }
                           break;
                        }
                     }
                  }
               }
            } else if (mevent.bstate & BUTTON3_RELEASED) {
               ch = KEY_RIGHTCLICK;
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
      case KEY_FOCUS_IN:
      case KEY_FOCUS_OUT:
         break;
      case KEY_LEFT:
      case KEY_CTRL('B'):
         if (this->panelCount < 2) {
            goto defaultHandler;
         }

         if (!this->allowFocusChange) {
            break;
         }

         if (focus > 0 && Panel_eventHandlerFn(panelFocus)) {
            Panel_eventHandler(panelFocus, EVENT_PANEL_LOST_FOCUS);
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

         if ((size_t)focus < this->panelCount - 1 && Panel_eventHandlerFn(panelFocus)) {
            Panel_eventHandler(panelFocus, EVENT_PANEL_LOST_FOCUS);
         }

tryRight:
         if ((size_t)focus < this->panelCount - 1) {
            focus++;
         }

         panelFocus = (Panel*) Vector_get(this->panels, focus);
         if (Panel_size(panelFocus) == 0 && (size_t)focus < this->panelCount - 1) {
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
