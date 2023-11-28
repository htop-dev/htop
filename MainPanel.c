/*
htop - ColumnsPanel.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "MainPanel.h"

#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Machine.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "Row.h"
#include "RowField.h"
#include "Settings.h"
#include "Table.h"
#include "XUtils.h"


static const char* const MainFunctions[]     = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  ", NULL};
static const char* const MainFunctions_ro[]  = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "      ", "      ", "      ", "Quit  ", NULL};

void MainPanel_updateLabels(MainPanel* this, bool list, bool filter) {
   FunctionBar* bar = MainPanel_getFunctionBar(this);
   FunctionBar_setLabel(bar, KEY_F(5), list   ? "List  " : "Tree  ");
   FunctionBar_setLabel(bar, KEY_F(4), filter ? "FILTER" : "Filter");
}

static void MainPanel_idSearch(MainPanel* this, int ch) {
   Panel* super = (Panel*) this;
   pid_t id = ch - 48 + this->idSearch;
   for (int i = 0; i < Panel_size(super); i++) {
      const Row* row = (const Row*) Panel_get(super, i);
      if (row && row->id == id) {
         Panel_setSelected(super, i);
         break;
      }
   }
   this->idSearch = id * 10;
   if (this->idSearch > 10000000) {
      this->idSearch = 0;
   }
}

static const char* MainPanel_getValue(Panel* this, int i) {
   Row* row = (Row*) Panel_get(this, i);
   return Row_sortKeyString(row);
}

static HandlerResult MainPanel_eventHandler(Panel* super, int ch) {
   MainPanel* this = (MainPanel*) super;
   Machine* host = this->state->host;
   Htop_Reaction reaction = HTOP_OK;
   HandlerResult result = IGNORED;

   /* Let supervising ScreenManager handle resize */
   if (ch == KEY_RESIZE)
      return IGNORED;

   /* reset on every normal key */
   bool needReset = ch != ERR;
   #ifdef HAVE_GETMOUSE
   /* except mouse events while mouse support is disabled */
   if (!(ch != KEY_MOUSE || host->settings->enableMouse))
      needReset = false;
   #endif
   if (needReset)
      this->state->hideSelection = false;

   Settings* settings = host->settings;
   ScreenSettings* ss = settings->ss;

   if (EVENT_IS_HEADER_CLICK(ch)) {
      int x = EVENT_HEADER_CLICK_GET_X(ch);
      int hx = super->scrollH + x + 1;
      RowField field = RowField_keyAt(settings, hx);
      if (ss->treeView && ss->treeViewAlwaysByPID) {
         ss->treeView = false;
         ss->direction = 1;
         reaction |= Action_setSortKey(settings, field);
      } else if (field == ScreenSettings_getActiveSortKey(ss)) {
         ScreenSettings_invertSortOrder(ss);
      } else {
         reaction |= Action_setSortKey(settings, field);
      }
      reaction |= HTOP_RECALCULATE | HTOP_REDRAW_BAR | HTOP_SAVE_SETTINGS;
      result = HANDLED;
   } else if (EVENT_IS_SCREEN_TAB_CLICK(ch)) {
      int x = EVENT_SCREEN_TAB_GET_X(ch);
      reaction |= Action_setScreenTab(this->state, x);
      result = HANDLED;
   } else if (ch != ERR && this->inc->active) {
      bool filterChanged = IncSet_handleKey(this->inc, ch, super, MainPanel_getValue, NULL);
      if (filterChanged) {
         host->activeTable->incFilter = IncSet_filter(this->inc);
         reaction = HTOP_REFRESH | HTOP_REDRAW_BAR;
      }
      if (this->inc->found) {
         reaction |= Action_follow(this->state);
         reaction |= HTOP_KEEP_FOLLOWING;
      }
      result = HANDLED;
   } else if (ch == 27) {
      this->state->hideSelection = true;
      return HANDLED;
   } else if (ch != ERR && ch > 0 && ch < KEY_MAX && this->keys[ch]) {
      reaction |= (this->keys[ch])(this->state);
      result = HANDLED;
   } else if (0 < ch && ch < 255 && isdigit((unsigned char)ch)) {
      MainPanel_idSearch(this, ch);
   } else {
      if (ch != ERR) {
         this->idSearch = 0;
      } else {
         reaction |= HTOP_KEEP_FOLLOWING;
      }
   }

   if ((reaction & HTOP_REDRAW_BAR) == HTOP_REDRAW_BAR) {
      MainPanel_updateLabels(this, settings->ss->treeView, host->activeTable->incFilter);
   }
   if ((reaction & HTOP_RESIZE) == HTOP_RESIZE) {
      result |= RESIZE;
   }
   if ((reaction & HTOP_UPDATE_PANELHDR) == HTOP_UPDATE_PANELHDR) {
      result |= REDRAW;
   }
   if ((reaction & HTOP_REFRESH) == HTOP_REFRESH) {
      result |= REFRESH;
   }
   if ((reaction & HTOP_RECALCULATE) == HTOP_RECALCULATE) {
      result |= RESCAN;
   }
   if ((reaction & HTOP_SAVE_SETTINGS) == HTOP_SAVE_SETTINGS) {
      host->settings->changed = true;
   }
   if ((reaction & HTOP_QUIT) == HTOP_QUIT) {
      return BREAK_LOOP;
   }
   if ((reaction & HTOP_KEEP_FOLLOWING) != HTOP_KEEP_FOLLOWING) {
      host->activeTable->following = -1;
      Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
   }
   return result;
}

int MainPanel_selectedRow(MainPanel* this) {
   const Row* row = (const Row*) Panel_getSelected((Panel*)this);
   return row ? row->id : -1;
}

bool MainPanel_foreachRow(MainPanel* this, MainPanel_foreachRowFn fn, Arg arg, bool* wasAnyTagged) {
   Panel* super = (Panel*) this;
   bool ok = true;
   bool anyTagged = false;
   for (int i = 0; i < Panel_size(super); i++) {
      Row* row = (Row*) Panel_get(super, i);
      if (row->tag) {
         ok &= fn(row, arg);
         anyTagged = true;
      }
   }
   if (!anyTagged) {
      Row* row = (Row*) Panel_getSelected(super);
      if (row) {
         ok &= fn(row, arg);
      }
   }

   if (wasAnyTagged)
      *wasAnyTagged = anyTagged;

   return ok;
}

static void MainPanel_drawFunctionBar(Panel* super, bool hideFunctionBar) {
   MainPanel* this = (MainPanel*) super;

   // Do not hide active search and filter bar.
   if (hideFunctionBar && !this->inc->active)
      return;

   IncSet_drawBar(this->inc, CRT_colors[FUNCTION_BAR]);
   if (this->state->pauseUpdate) {
      FunctionBar_append("PAUSED", CRT_colors[PAUSED]);
   }
}

static void MainPanel_printHeader(Panel* super) {
   MainPanel* this = (MainPanel*) super;
   Machine* host = this->state->host;
   Table_printHeader(host->settings, &super->header);
}

const PanelClass MainPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = MainPanel_delete
   },
   .eventHandler = MainPanel_eventHandler,
   .drawFunctionBar = MainPanel_drawFunctionBar,
   .printHeader = MainPanel_printHeader
};

MainPanel* MainPanel_new(void) {
   MainPanel* this = AllocThis(MainPanel);
   this->processBar = FunctionBar_new(MainFunctions, NULL, NULL);
   this->readonlyBar = FunctionBar_new(MainFunctions_ro, NULL, NULL);
   FunctionBar* activeBar = Settings_isReadonly() ? this->readonlyBar : this->processBar;
   Panel_init((Panel*) this, 1, 1, 1, 1, Class(Row), false, activeBar);
   this->keys = xCalloc(KEY_MAX, sizeof(Htop_Action));
   this->inc = IncSet_new(activeBar);

   Action_setBindings(this->keys);
   Platform_setBindings(this->keys);

   return this;
}

void MainPanel_setState(MainPanel* this, State* state) {
   this->state = state;
}

void MainPanel_setFunctionBar(MainPanel* this, bool readonly) {
   this->super.defaultBar = readonly ? this->readonlyBar : this->processBar;
   this->inc->defaultBar = this->super.defaultBar;
}

void MainPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   MainPanel* this = (MainPanel*) object;
   MainPanel_setFunctionBar(this, false);
   FunctionBar_delete(this->readonlyBar);
   Panel_done(super);
   IncSet_delete(this->inc);
   free(this->keys);
   free(this);
}
