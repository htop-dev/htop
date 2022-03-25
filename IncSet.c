/*
htop - IncSet.c
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "IncSet.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "CRT.h"
#include "ListItem.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static void IncMode_reset(IncMode* mode) {
   mode->index = 0;
   mode->buffer[0] = 0;
}

void IncSet_reset(IncSet* this, IncType type) {
   IncMode_reset(&this->modes[type]);
}

void IncSet_setFilter(IncSet* this, const char* filter) {
   IncMode* mode = &this->modes[INC_FILTER];
   size_t len = String_safeStrncpy(mode->buffer, filter, sizeof(mode->buffer));
   mode->index = len;
   this->filtering = true;
}

static const char* const searchFunctions[] = {"Next  ", "Prev   ", "Cancel ", " Search: ", NULL};
static const char* const searchKeys[] = {"F3", "S-F3", "Esc", "  "};
static const int searchEvents[] = {KEY_F(3), KEY_F(15), 27, ERR};

static inline void IncMode_initSearch(IncMode* search) {
   memset(search, 0, sizeof(IncMode));
   search->bar = FunctionBar_new(searchFunctions, searchKeys, searchEvents);
   search->isFilter = false;
}

static const char* const filterFunctions[] = {"Done  ", "Clear ", " Filter: ", NULL};
static const char* const filterKeys[] = {"Enter", "Esc", "  "};
static const int filterEvents[] = {13, 27, ERR};

static inline void IncMode_initFilter(IncMode* filter) {
   memset(filter, 0, sizeof(IncMode));
   filter->bar = FunctionBar_new(filterFunctions, filterKeys, filterEvents);
   filter->isFilter = true;
}

static inline void IncMode_done(IncMode* mode) {
   FunctionBar_delete(mode->bar);
}

IncSet* IncSet_new(FunctionBar* bar) {
   IncSet* this = xMalloc(sizeof(IncSet));
   IncMode_initSearch(&(this->modes[INC_SEARCH]));
   IncMode_initFilter(&(this->modes[INC_FILTER]));
   this->active = NULL;
   this->defaultBar = bar;
   this->filtering = false;
   this->found = false;
   return this;
}

void IncSet_delete(IncSet* this) {
   IncMode_done(&(this->modes[0]));
   IncMode_done(&(this->modes[1]));
   free(this);
}

static void updateWeakPanel(const IncSet* this, Panel* panel, Vector* lines) {
   const Object* selected = Panel_getSelected(panel);
   Panel_prune(panel);
   if (this->filtering) {
      int n = 0;
      const char* incFilter = this->modes[INC_FILTER].buffer;
      for (int i = 0; i < Vector_size(lines); i++) {
         ListItem* line = (ListItem*)Vector_get(lines, i);
         if (String_contains_i(line->value, incFilter, true)) {
            Panel_add(panel, (Object*)line);
            if (selected == (Object*)line) {
               Panel_setSelected(panel, n);
            }

            n++;
         }
      }
   } else {
      for (int i = 0; i < Vector_size(lines); i++) {
         Object* line = Vector_get(lines, i);
         Panel_add(panel, line);
         if (selected == line) {
            Panel_setSelected(panel, i);
         }
      }
   }
}

static bool search(const IncSet* this, Panel* panel, IncMode_GetPanelValue getPanelValue) {
   int size = Panel_size(panel);
   for (int i = 0; i < size; i++) {
      if (String_contains_i(getPanelValue(panel, i), this->active->buffer, true)) {
         Panel_setSelected(panel, i);
         return true;
      }
   }

   return false;
}

void IncSet_activate(IncSet* this, IncType type, Panel* panel) {
   this->active = &(this->modes[type]);
   panel->currentBar = this->active->bar;
   panel->cursorOn = true;
   this->panel = panel;
   IncSet_drawBar(this, CRT_colors[FUNCTION_BAR]);
}

static void IncSet_deactivate(IncSet* this, Panel* panel) {
   this->active = NULL;
   Panel_setDefaultBar(panel);
   panel->cursorOn = false;
   FunctionBar_draw(this->defaultBar);
}

static bool IncMode_find(const IncMode* mode, Panel* panel, IncMode_GetPanelValue getPanelValue, int step) {
   int size = Panel_size(panel);
   int here = Panel_getSelectedIndex(panel);
   int i = here;
   for (;;) {
      i += step;
      if (i == size) {
         i = 0;
      }
      if (i == -1) {
         i = size - 1;
      }
      if (i == here) {
         return false;
      }

      if (String_contains_i(getPanelValue(panel, i), mode->buffer, true)) {
         Panel_setSelected(panel, i);
         return true;
      }
   }
}

bool IncSet_handleKey(IncSet* this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines) {
   if (ch == ERR)
      return true;

   IncMode* mode = this->active;
   int size = Panel_size(panel);
   bool filterChanged = false;
   bool doSearch = true;
   if (ch == KEY_F(3) || ch == KEY_F(15)) {
      if (size == 0)
         return true;

      IncMode_find(mode, panel, getPanelValue, ch == KEY_F(3) ? 1 : -1);
      doSearch = false;
   } else if (0 < ch && ch < 255 && isprint((unsigned char)ch)) {
      if (mode->index < INCMODE_MAX) {
         mode->buffer[mode->index] = (char) ch;
         mode->index++;
         mode->buffer[mode->index] = 0;
         if (mode->isFilter) {
            filterChanged = true;
            if (mode->index == 1) {
               this->filtering = true;
            }
         }
      }
   } else if (ch == KEY_BACKSPACE || ch == 127) {
      if (mode->index > 0) {
         mode->index--;
         mode->buffer[mode->index] = 0;
         if (mode->isFilter) {
            filterChanged = true;
            if (mode->index == 0) {
               this->filtering = false;
               IncMode_reset(mode);
            }
         }
      } else {
         doSearch = false;
      }
   } else if (ch == KEY_RESIZE) {
      doSearch = (mode->index > 0);
   } else {
      if (mode->isFilter) {
         filterChanged = true;
         if (ch == 27) {
            this->filtering = false;
            IncMode_reset(mode);
         }
      } else {
         if (ch == 27) {
            IncMode_reset(mode);
         }
      }
      IncSet_deactivate(this, panel);
      doSearch = false;
   }
   if (doSearch) {
      this->found = search(this, panel, getPanelValue);
   }
   if (filterChanged && lines) {
      updateWeakPanel(this, panel, lines);
   }
   return filterChanged;
}

const char* IncSet_getListItemValue(Panel* panel, int i) {
   const ListItem* l = (const ListItem*) Panel_get(panel, i);
   return l ? l->value : "";
}

void IncSet_drawBar(const IncSet* this, int attr) {
   if (this->active) {
      if (!this->active->isFilter && !this->found)
         attr = CRT_colors[FAILED_SEARCH];
      int cursorX = FunctionBar_drawExtra(this->active->bar, this->active->buffer, attr, true);
      this->panel->cursorY = LINES - 1;
      this->panel->cursorX = cursorX;
   } else {
      FunctionBar_draw(this->defaultBar);
   }
}

int IncSet_synthesizeEvent(IncSet* this, int x) {
   if (this->active) {
      return FunctionBar_synthesizeEvent(this->active->bar, x);
   } else {
      return FunctionBar_synthesizeEvent(this->defaultBar, x);
   }
}
