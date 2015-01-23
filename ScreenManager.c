/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreenManager.h"
#include "ProcessList.h"

#include "Object.h"

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

/*{
#include "FunctionBar.h"
#include "Vector.h"
#include "Header.h"
#include "Settings.h"
#include "Panel.h"

typedef enum Orientation_ {
   VERTICAL,
   HORIZONTAL
} Orientation;

typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Orientation orientation;
   Vector* panels;
   Vector* fuBars;
   int panelCount;
   const FunctionBar* fuBar;
   const Header* header;
   const Settings* settings;
   bool owner;
   bool allowFocusChange;
} ScreenManager;

}*/

ScreenManager* ScreenManager_new(int x1, int y1, int x2, int y2, Orientation orientation, const Header* header, const Settings* settings, bool owner) {
   ScreenManager* this;
   this = malloc(sizeof(ScreenManager));
   this->x1 = x1;
   this->y1 = y1;
   this->x2 = x2;
   this->y2 = y2;
   this->fuBar = NULL;
   this->orientation = orientation;
   this->panels = Vector_new(Class(Panel), owner, DEFAULT_SIZE);
   this->fuBars = Vector_new(Class(FunctionBar), true, DEFAULT_SIZE);
   this->panelCount = 0;
   this->header = header;
   this->settings = settings;
   this->owner = owner;
   this->allowFocusChange = true;
   return this;
}

void ScreenManager_delete(ScreenManager* this) {
   Vector_delete(this->panels);
   Vector_delete(this->fuBars);
   free(this);
}

inline int ScreenManager_size(ScreenManager* this) {
   return this->panelCount;
}

void ScreenManager_add(ScreenManager* this, Panel* item, FunctionBar* fuBar, int size) {
   if (this->orientation == HORIZONTAL) {
      int lastX = 0;
      if (this->panelCount > 0) {
         Panel* last = (Panel*) Vector_get(this->panels, this->panelCount - 1);
         lastX = last->x + last->w + 1;
      }
      int height = LINES - this->y1 + this->y2;
      if (size > 0) {
         Panel_resize(item, size, height);
      } else {
         Panel_resize(item, COLS-this->x1+this->x2-lastX, height);
      }
      Panel_move(item, lastX, this->y1);
   }
   // TODO: VERTICAL
   Vector_add(this->panels, item);
   if (fuBar)
      Vector_add(this->fuBars, fuBar);
   else
      Vector_add(this->fuBars, FunctionBar_new(NULL, NULL, NULL));
   if (!this->fuBar && fuBar) this->fuBar = fuBar;
   item->needsRedraw = true;
   this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* this, int idx) {
   assert(this->panelCount > idx);
   Panel* panel = (Panel*) Vector_remove(this->panels, idx);
   Vector_remove(this->fuBars, idx);
   this->fuBar = NULL;
   this->panelCount--;
   return panel;
}

void ScreenManager_resize(ScreenManager* this, int x1, int y1, int x2, int y2) {
   this->x1 = x1;
   this->y1 = y1;
   this->x2 = x2;
   this->y2 = y2;
   int panels = this->panelCount;
   if (this->orientation == HORIZONTAL) {
      int lastX = 0;
      for (int i = 0; i < panels - 1; i++) {
         Panel* panel = (Panel*) Vector_get(this->panels, i);
         Panel_resize(panel, panel->w, LINES-y1+y2);
         Panel_move(panel, lastX, y1);
         lastX = panel->x + panel->w + 1;
      }
      Panel* panel = (Panel*) Vector_get(this->panels, panels-1);
      Panel_resize(panel, COLS-x1+x2-lastX, LINES-y1+y2);
      Panel_move(panel, lastX, y1);
   }
   // TODO: VERTICAL
}

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey) {
   bool quit = false;
   int focus = 0;
   
   Panel* panelFocus = (Panel*) Vector_get(this->panels, focus);
   if (this->fuBar)
      FunctionBar_draw(this->fuBar, NULL);

   struct timeval tv;
   double oldTime = 0.0;

   int ch = ERR;
   int closeTimeout = 0;

   bool drawPanel = true;
   bool timeToRecalculate = true;
   bool doRefresh = true;
   bool forceRecalculate = false;
   int sortTimeout = 0;
   int resetSortTimeout = 5;

   while (!quit) {
      int panels = this->panelCount;
      if (this->header) {
         gettimeofday(&tv, NULL);
         double newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);
         timeToRecalculate = (newTime - oldTime > this->settings->delay);
         if (newTime < oldTime) timeToRecalculate = true; // clock was adjusted?
//fprintf(stderr, "\n%p %f ", this, newTime);
         if (doRefresh) {
            if (timeToRecalculate || forceRecalculate) {
               ProcessList_scan(this->header->pl);
//fprintf(stderr, "scan ");
            }
//fprintf(stderr, "sortTo=%d ", sortTimeout);
            if (sortTimeout == 0 || this->settings->treeView) {
               ProcessList_sort(this->header->pl);
//fprintf(stderr, "sort ");
               sortTimeout = 1;
            }
            //this->header->pl->incFilter = IncSet_filter(inc);
            ProcessList_rebuildPanel(this->header->pl);
//fprintf(stderr, "rebuild ");
            drawPanel = true;
         }
         if (timeToRecalculate || forceRecalculate) {
            Header_draw(this->header);
//fprintf(stderr, "drawHeader ");
            oldTime = newTime;
            forceRecalculate = false;
         }
         doRefresh = true;
      }
      
      if (drawPanel) {
         for (int i = 0; i < panels; i++) {
            Panel* panel = (Panel*) Vector_get(this->panels, i);
            Panel_draw(panel, i == focus);
//fprintf(stderr, "drawPanel ");
            if (i < panels) {
               if (this->orientation == HORIZONTAL) {
                  mvvline(panel->y, panel->x+panel->w, ' ', panel->h+1);
               }
            }
         }
      }
      
      FunctionBar* bar = (FunctionBar*) Vector_get(this->fuBars, focus);
      if (bar)
         this->fuBar = bar;
      if (this->fuBar)
         FunctionBar_draw(this->fuBar, NULL);

      int prevCh = ch;
      ch = getch();

//fprintf(stderr, "ch=%d ", ch);
      
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.y == LINES - 1) {
               ch = FunctionBar_synthesizeEvent(this->fuBar, mevent.x);
            } else {
               for (int i = 0; i < this->panelCount; i++) {
                  Panel* panel = (Panel*) Vector_get(this->panels, i);
                  if (mevent.x > panel->x && mevent.x <= panel->x+panel->w &&
                     mevent.y > panel->y && mevent.y <= panel->y+panel->h &&
                     (this->allowFocusChange || panelFocus == panel) ) {
                     focus = i;
                     panelFocus = panel;
                     Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                     break;
                  }
               }
            }
         }
      }
      if (Panel_eventHandlerFn(panelFocus)) {
         HandlerResult result = Panel_eventHandler(panelFocus, ch);
//fprintf(stderr, "eventResult=%d ", result);
         if (result & REFRESH) {
            doRefresh = true;
            sortTimeout = 0;
         }
         if (result & RECALCULATE) {
            forceRecalculate = true;
            sortTimeout = 0;
         }
         if (result & HANDLED) {
            drawPanel = true;
            continue;
         } else if (result & BREAK_LOOP) {
            quit = true;
            continue;
         }
      }
      if (ch == ERR) {
         sortTimeout--;
         if (prevCh == ch && !timeToRecalculate) {
            closeTimeout++;
            if (closeTimeout == 100) {
               break;
            }
         } else
            closeTimeout = 0;
         drawPanel = false;
//fprintf(stderr, "err ");
         continue;
      }
      drawPanel = true;
      
      switch (ch) {
      case KEY_RESIZE:
      {
         ScreenManager_resize(this, this->x1, this->y1, this->x2, this->y2);
         continue;
      }
      case KEY_LEFT:
      case KEY_CTRLB:
         if (!this->allowFocusChange)
            break;
         tryLeft:
         if (focus > 0)
            focus--;
         panelFocus = (Panel*) Vector_get(this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus > 0)
            goto tryLeft;
         break;
      case KEY_RIGHT:
      case KEY_CTRLF:
      case 9:
         if (!this->allowFocusChange)
            break;
         tryRight:
         if (focus < this->panelCount - 1)
            focus++;
         panelFocus = (Panel*) Vector_get(this->panels, focus);
         if (Panel_size(panelFocus) == 0 && focus < this->panelCount - 1)
            goto tryRight;
         break;
      case KEY_F(10):
      case 'q':
      case 27:
         quit = true;
         continue;
      default:
//fprintf(stderr, "onKey ");
         sortTimeout = resetSortTimeout;
         Panel_onKey(panelFocus, ch);
         break;
      }
//fprintf(stderr, "loop ");
   }

   if (lastFocus)
      *lastFocus = panelFocus;
   if (lastKey)
      *lastKey = ch;
}
