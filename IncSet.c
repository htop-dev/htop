/*
htop - IncSet.c
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "IncSet.h"

#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "History.h"
#include "LineEditor.h"
#include "ListItem.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static void IncMode_reset(IncMode* mode) {
   LineEditor_reset(&mode->editor);
}

void IncSet_reset(IncSet* this, IncType type) {
   IncMode_reset(&this->modes[type]);
}

void IncSet_setFilter(IncSet* this, const char* filter) {
   IncMode* mode = &this->modes[INC_FILTER];
   LineEditor_setText(&mode->editor, filter);
   this->filtering = (filter && filter[0] != '\0');
}

static const char* const searchFunctions[] = {"Next  ", "Prev   ", "Cancel ", " Search: ", NULL};
static const char* const searchKeys[] = {"F3", "S-F3", "Esc", "  "};
static const int searchEvents[] = {KEY_F(3), KEY_F(15), 27, ERR};

static inline void IncMode_initSearch(IncMode* search) {
   memset(search, 0, sizeof(IncMode));
   search->bar = FunctionBar_new(searchFunctions, searchKeys, searchEvents);
   search->isFilter = false;
   LineEditor_init(&search->editor);
}

static const char* const filterFunctions[] = {"Done  ", "Clear ", " Filter: ", NULL};
static const char* const filterKeys[] = {"Enter", "Esc", "  "};
static const int filterEvents[] = {13, 27, ERR};

static inline void IncMode_initFilter(IncMode* filter) {
   memset(filter, 0, sizeof(IncMode));
   filter->bar = FunctionBar_new(filterFunctions, filterKeys, filterEvents);
   filter->isFilter = true;
   LineEditor_init(&filter->editor);
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
   this->history = NULL;
   return this;
}

void IncSet_delete(IncSet* this) {
   IncMode_done(&(this->modes[0]));
   IncMode_done(&(this->modes[1]));
   if (this->history)
      History_delete(this->history);
   free(this);
}

void IncSet_setHistoryFile(IncSet* this, const char* filename) {
   if (this->history)
      History_delete(this->history);
   this->history = History_new(filename);
}

void IncSet_saveHistory(const IncSet* this) {
   if (this->history)
      History_save(this->history);
}

static void updateWeakPanel(IncSet* this, Panel* panel, Vector* lines) {
   const Object* selected = Panel_getSelected(panel);
   Panel_prune(panel);
   if (this->filtering) {
      int n = 0;
      const char* incFilter = LineEditor_getText(&this->modes[INC_FILTER].editor);
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

static bool search(IncSet* this, Panel* panel, IncMode_GetPanelValue getPanelValue) {
   int size = Panel_size(panel);
   for (int i = 0; i < size; i++) {
      if (String_contains_i(getPanelValue(panel, i), LineEditor_getText(&this->active->editor), true)) {
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
   /* Reset history browse position when starting a new search/filter */
   if (this->history)
      History_resetPosition(this->history);
   IncSet_drawBar(this, CRT_colors[FUNCTION_BAR]);
}

static void IncSet_deactivate(IncSet* this, Panel* panel) {
   this->active = NULL;
   Panel_setDefaultBar(panel);
   panel->cursorOn = false;
   FunctionBar_draw(this->defaultBar);
}

static bool IncMode_find(IncMode* mode, Panel* panel, IncMode_GetPanelValue getPanelValue, int step) {
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

      if (String_contains_i(getPanelValue(panel, i), LineEditor_getText(&mode->editor), true)) {
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

   /* Mouse click in the function bar input field: place cursor */
   if (ch == KEY_MOUSE_BAR_CLICK) {
      int fieldStartX = FunctionBar_getWidth(mode->bar);
      LineEditor_click(&mode->editor, panel->lastMouseBarClickX, fieldStartX);
      IncSet_drawBar(this, CRT_colors[FUNCTION_BAR]);
      return false;
   }

   if (ch == KEY_F(3) || ch == KEY_F(15)) {
      if (size == 0)
         return true;

      IncMode_find(mode, panel, getPanelValue, ch == KEY_F(3) ? 1 : -1);
      doSearch = false;
   } else if (ch == KEY_UP) {
      /* History navigation: older entry */
      if (this->history) {
         const char* entry = History_navigate(this->history, &mode->editor, true);
         if (entry) {
            LineEditor_setText(&mode->editor, entry);
            if (mode->isFilter) {
               filterChanged = true;
               this->filtering = (LineEditor_getText(&mode->editor)[0] != '\0');
            }
         }
         doSearch = !mode->isFilter;
      } else {
         doSearch = false;
      }
   } else if (ch == KEY_DOWN) {
      /* History navigation: newer entry */
      if (this->history) {
         const char* entry = History_navigate(this->history, &mode->editor, false);
         if (entry) {
            LineEditor_setText(&mode->editor, entry);
            if (mode->isFilter) {
               filterChanged = true;
               this->filtering = (LineEditor_getText(&mode->editor)[0] != '\0');
            }
         }
         doSearch = !mode->isFilter;
      } else {
         doSearch = false;
      }
   } else if (ch == KEY_RESIZE) {
      doSearch = (LineEditor_getText(&mode->editor)[0] != '\0');
   } else if (ch == 13 || ch == '\r' || ch == KEY_ENTER) {
      /* Enter confirms: add to history and deactivate */
      if (this->history) {
         const char* text = LineEditor_getText(&mode->editor);
         if (text[0] != '\0') {
            History_add(this->history, text);
            History_save(this->history);
         }
         History_resetPosition(this->history);
      }
      if (!mode->isFilter) {
         /* For search: reset buffer on Enter */
         IncMode_reset(mode);
      }
      IncSet_deactivate(this, panel);
      doSearch = false;
      filterChanged = mode->isFilter;
   } else if (ch == 27) {
      /* Esc aborts */
      if (this->history)
         History_resetPosition(this->history);
      if (mode->isFilter) {
         filterChanged = true;
         this->filtering = false;
         IncMode_reset(mode);
      } else {
         IncMode_reset(mode);
      }
      IncSet_deactivate(this, panel);
      doSearch = false;
   } else {
      /* Try line editor first */
      bool textChanged = LineEditor_handleKey(&mode->editor, ch);
      if (textChanged) {
         if (mode->isFilter) {
            filterChanged = true;
            const char* buf = LineEditor_getText(&mode->editor);
            this->filtering = (buf[0] != '\0');
         }
      } else {
         /* Key was a movement key (no text change) or unrecognized */
         doSearch = false;
      }
   }

   if (doSearch && LineEditor_getText(&mode->editor)[0] != '\0') {
      this->found = search(this, panel, getPanelValue);
   }
   if (filterChanged && lines) {
      updateWeakPanel(this, panel, lines);
   }

   /* Redraw the bar to reflect cursor / scroll position changes */
   if (this->active) {
      IncSet_drawBar(this, CRT_colors[FUNCTION_BAR]);
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

      /* Draw the function keys and get the start of the input field */
      int fieldStartX = FunctionBar_drawExtra(this->active->bar, NULL, -1, false);

      /* Update scroll so the cursor remains visible */
      int fieldWidth = COLS - fieldStartX;
      if (fieldWidth < 1) fieldWidth = 1;
      LineEditor_updateScroll(&this->active->editor, fieldWidth);

      /* Draw the visible portion of the input text */
      int cursorX = LineEditor_draw(&this->active->editor, fieldStartX, fieldWidth, attr);

      curs_set(1);

      this->panel->cursorY = LINES - 1;
      this->panel->cursorX = cursorX;
   } else {
      FunctionBar_draw(this->defaultBar);
   }
}

int IncSet_synthesizeEvent(IncSet* this, int x) {
   if (this->active) {
      int ev = FunctionBar_synthesizeEvent(this->active->bar, x);
      /* Click in the input area: synthesize a bar-click event */
      if (ev == ERR && x >= FunctionBar_getWidth(this->active->bar)) {
         this->panel->lastMouseBarClickX = x;
         return KEY_MOUSE_BAR_CLICK;
      }
      return ev;
   } else {
      return FunctionBar_synthesizeEvent(this->defaultBar, x);
   }
}
