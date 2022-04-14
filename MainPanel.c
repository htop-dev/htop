/*
htop - ColumnsPanel.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "MainPanel.h"

#include <ctype.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "Settings.h"
#include "XUtils.h"


static const char* const MainFunctions[]     = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  ", NULL};
static const char* const MainFunctions_ro[]  = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "      ", "      ", "      ", "Quit  ", NULL};

void MainPanel_updateLabels(MainPanel* this, bool list, bool filter) {
   FunctionBar* bar = MainPanel_getFunctionBar(this);
   FunctionBar_setLabel(bar, KEY_F(5), list   ? "List  " : "Tree  ");
   FunctionBar_setLabel(bar, KEY_F(4), filter ? "FILTER" : "Filter");
}

static void MainPanel_pidSearch(MainPanel* this, int ch) {
   Panel* super = (Panel*) this;
   pid_t pid = ch - 48 + this->pidSearch;
   for (int i = 0; i < Panel_size(super); i++) {
      const Process* p = (const Process*) Panel_get(super, i);
      if (p && p->pid == pid) {
         Panel_setSelected(super, i);
         break;
      }
   }
   this->pidSearch = pid * 10;
   if (this->pidSearch > 10000000) {
      this->pidSearch = 0;
   }
}

static const char* MainPanel_getValue(Panel* this, int i) {
   const Process* p = (const Process*) Panel_get(this, i);
   return Process_getCommand(p);
}

static HandlerResult MainPanel_eventHandler(Panel* super, int ch) {
   MainPanel* this = (MainPanel*) super;

   HandlerResult result = IGNORED;

   Htop_Reaction reaction = HTOP_OK;

   /* Let supervising ScreenManager handle resize */
   if (ch == KEY_RESIZE)
      return IGNORED;

   /* reset on every normal key */
   bool needReset = ch != ERR;
   #ifdef HAVE_GETMOUSE
   /* except mouse events while mouse support is disabled */
   if (!(ch != KEY_MOUSE || this->state->settings->enableMouse))
      needReset = false;
   #endif
   if (needReset)
      this->state->hideProcessSelection = false;

   Settings* settings = this->state->settings;
   ScreenSettings* ss = settings->ss;

   if (EVENT_IS_HEADER_CLICK(ch)) {
      int x = EVENT_HEADER_CLICK_GET_X(ch);
      const ProcessList* pl = this->state->pl;
      int hx = super->scrollH + x + 1;
      ProcessField field = ProcessList_keyAt(pl, hx);
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
      reaction |= Action_setScreenTab(settings, x);
      result = HANDLED;
   } else if (ch != ERR && this->inc->active) {
      bool filterChanged = IncSet_handleKey(this->inc, ch, super, MainPanel_getValue, NULL);
      if (filterChanged) {
         this->state->pl->incFilter = IncSet_filter(this->inc);
         reaction = HTOP_REFRESH | HTOP_REDRAW_BAR;
      }
      if (this->inc->found) {
         reaction |= Action_follow(this->state);
         reaction |= HTOP_KEEP_FOLLOWING;
      }
      result = HANDLED;
   } else if (ch == 27) {
      this->state->hideProcessSelection = true;
      return HANDLED;
   } else if (ch != ERR && ch > 0 && ch < KEY_MAX && this->keys[ch]) {
      reaction |= (this->keys[ch])(this->state);
      result = HANDLED;
   } else if (0 < ch && ch < 255 && isdigit((unsigned char)ch)) {
      MainPanel_pidSearch(this, ch);
   } else {
      if (ch != ERR) {
         this->pidSearch = 0;
      } else {
         reaction |= HTOP_KEEP_FOLLOWING;
      }
   }

   if (reaction & HTOP_REDRAW_BAR) {
      MainPanel_updateLabels(this, settings->ss->treeView, this->state->pl->incFilter);
   }
   if (reaction & HTOP_RESIZE) {
      result |= RESIZE;
   }
   if (reaction & HTOP_UPDATE_PANELHDR) {
      result |= REDRAW;
   }
   if (reaction & HTOP_REFRESH) {
      result |= REFRESH;
   }
   if (reaction & HTOP_RECALCULATE) {
      result |= RESCAN;
   }
   if (reaction & HTOP_SAVE_SETTINGS) {
      this->state->settings->changed = true;
   }
   if (reaction & HTOP_QUIT) {
      return BREAK_LOOP;
   }
   if (!(reaction & HTOP_KEEP_FOLLOWING)) {
      this->state->pl->following = -1;
      Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
   }
   return result;
}

int MainPanel_selectedPid(MainPanel* this) {
   const Process* p = (const Process*) Panel_getSelected((Panel*)this);
   if (p) {
      return p->pid;
   }
   return -1;
}

bool MainPanel_foreachProcess(MainPanel* this, MainPanel_ForeachProcessFn fn, Arg arg, bool* wasAnyTagged) {
   Panel* super = (Panel*) this;
   bool ok = true;
   bool anyTagged = false;
   for (int i = 0; i < Panel_size(super); i++) {
      Process* p = (Process*) Panel_get(super, i);
      if (p->tag) {
         ok = fn(p, arg) && ok;
         anyTagged = true;
      }
   }
   if (!anyTagged) {
      Process* p = (Process*) Panel_getSelected(super);
      if (p) {
         ok &= fn(p, arg);
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
   if (this->state->pauseProcessUpdate) {
      FunctionBar_append("PAUSED", CRT_colors[PAUSED]);
   }
}

static void MainPanel_printHeader(Panel* super) {
   MainPanel* this = (MainPanel*) super;
   ProcessList_printHeader(this->state->pl, &super->header);
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

MainPanel* MainPanel_new() {
   MainPanel* this = AllocThis(MainPanel);
   Panel_init((Panel*) this, 1, 1, 1, 1, Class(Process), false, FunctionBar_new(Settings_isReadonly() ? MainFunctions_ro : MainFunctions, NULL, NULL));
   this->keys = xCalloc(KEY_MAX, sizeof(Htop_Action));
   this->inc = IncSet_new(MainPanel_getFunctionBar(this));

   Action_setBindings(this->keys);
   Platform_setBindings(this->keys);

   return this;
}

void MainPanel_setState(MainPanel* this, State* state) {
   this->state = state;
}

void MainPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   MainPanel* this = (MainPanel*) object;
   Panel_done(super);
   IncSet_delete(this->inc);
   free(this->keys);
   free(this);
}
