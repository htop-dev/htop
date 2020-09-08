/*
htop - ColumnsPanel.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MainPanel.h"
#include "Process.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>

static const char* const MainFunctions[]  = {"Help   ", "Setup  ", "Search ", "Filter ", "Tree   ", "SortBy ", "Nice - ", "Nice + ", "Kill   ", "Quit   ", NULL};

void MainPanel_updateTreeFunctions(MainPanel* this, bool mode) {
   FunctionBar* bar = MainPanel_getFunctionBar(this);
   if (mode) {
      FunctionBar_setLabel(bar, KEY_F(5), "Sorted");
      FunctionBar_setLabel(bar, KEY_F(6), "Collap");
   } else {
      FunctionBar_setLabel(bar, KEY_F(5), "Tree  ");
      FunctionBar_setLabel(bar, KEY_F(6), "SortBy");
   }
}

void MainPanel_pidSearch(MainPanel* this, int ch) {
   Panel* super = (Panel*) this;
   pid_t pid = ch-48 + this->pidSearch;
   for (int i = 0; i < Panel_size(super); i++) {
      Process* p = (Process*) Panel_get(super, i);
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

static HandlerResult MainPanel_eventHandler(Panel* super, int ch) {
   MainPanel* this = (MainPanel*) super;

   HandlerResult result = IGNORED;

   Htop_Reaction reaction = HTOP_OK;

   if (EVENT_IS_HEADER_CLICK(ch)) {
      int x = EVENT_HEADER_CLICK_GET_X(ch);
      ProcessList* pl = this->state->pl;
      Settings* settings = this->state->settings;
      int hx = super->scrollH + x + 1;
      ProcessField field = ProcessList_keyAt(pl, hx);
      if (field == settings->sortKey) {
         Settings_invertSortOrder(settings);
         settings->treeView = false;
      } else {
         reaction |= Action_setSortKey(settings, field);
      }
      reaction |= HTOP_RECALCULATE | HTOP_REDRAW_BAR | HTOP_SAVE_SETTINGS;
      result = HANDLED;
   } else if (ch != ERR && this->inc->active) {
      bool filterChanged = IncSet_handleKey(this->inc, ch, super, (IncMode_GetPanelValue) MainPanel_getValue, NULL);
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
      return HANDLED;
   } else if (ch != ERR && ch > 0 && ch < KEY_MAX && this->keys[ch]) {
      reaction |= (this->keys[ch])(this->state);
      result = HANDLED;
   } else if (isdigit(ch)) {
      MainPanel_pidSearch(this, ch);
   } else {
      if (ch != ERR) {
         this->pidSearch = 0;
      } else {
         reaction |= HTOP_KEEP_FOLLOWING;
      }
   }

   if (reaction & HTOP_REDRAW_BAR) {
      MainPanel_updateTreeFunctions(this, this->state->settings->treeView);
      IncSet_drawBar(this->inc);
   }
   if (reaction & HTOP_UPDATE_PANELHDR) {
      ProcessList_printHeader(this->state->pl, Panel_getHeader(super));
   }
   if (reaction & HTOP_REFRESH) {
      result |= REDRAW;
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
      Panel_setSelectionColor(super, CRT_colors[PANEL_SELECTION_FOCUS]);
   }
   return result;
}

int MainPanel_selectedPid(MainPanel* this) {
   Process* p = (Process*) Panel_getSelected((Panel*)this);
   if (p) {
      return p->pid;
   }
   return -1;
}

const char* MainPanel_getValue(MainPanel* this, int i) {
   Process* p = (Process*) Panel_get((Panel*)this, i);
   if (p)
      return p->comm;
   return "";
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
      if (p) ok = fn(p, arg) && ok;
   }
   if (wasAnyTagged)
      *wasAnyTagged = anyTagged;
   return ok;
}

PanelClass MainPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = MainPanel_delete
   },
   .eventHandler = MainPanel_eventHandler
};

MainPanel* MainPanel_new() {
   MainPanel* this = AllocThis(MainPanel);
   Panel_init((Panel*) this, 1, 1, 1, 1, Class(Process), false, FunctionBar_new(MainFunctions, NULL, NULL));
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
