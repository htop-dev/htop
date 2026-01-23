/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Action.h"

#include <assert.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "CategoriesPanel.h"
#include "CommandScreen.h"
#include "DynamicColumn.h"
#include "EnvScreen.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "IncSet.h"
#include "InfoScreen.h"
#include "ListItem.h"
#include "Macros.h"
#include "MainPanel.h"
#include "MemoryMeter.h"
#include "OpenFilesScreen.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "ProvideCurses.h"
#include "Row.h"
#include "RowField.h"
#include "Scheduling.h"
#include "ScreenManager.h"
#include "SignalsPanel.h"
#include "Table.h"
#include "TraceScreen.h"
#include "UsersTable.h"
#include "Vector.h"
#include "XUtils.h"

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
#include "Affinity.h"
#include "AffinityPanel.h"
#endif


Object* Action_pickFromVector(State* st, Panel* list, int x, bool follow) {
   MainPanel* mainPanel = st->mainPanel;
   Header* header = st->header;
   Machine* host = st->host;

   int y = ((Panel*)mainPanel)->y;
   ScreenManager* scr = ScreenManager_new(header, host, st, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, x);
   ScreenManager_add(scr, (Panel*)mainPanel, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   int row = follow ? MainPanel_selectedRow(mainPanel) : -1;
   if (follow && host->activeTable->following == -1) {
      host->activeTable->following = row;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch, NULL);
   if (unfollow) {
      host->activeTable->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move((Panel*)mainPanel, 0, y);
   Panel_resize((Panel*)mainPanel, COLS, LINES - y - 1);
   if (panelFocus == list && ch == 13) {
      if (follow) {
         const Row* selected = (const Row*)Panel_getSelected((Panel*)mainPanel);
         if (selected && selected->id == row)
            return Panel_getSelected(list);

         beep();
      } else {
         return Panel_getSelected(list);
      }
   }

   return NULL;
}

// ----------------------------------------

static void Action_runSetup(State* st) {
   const Settings* settings = st->host->settings;
   ScreenManager* scr = ScreenManager_new(st->header, st->host, st, true);
   CategoriesPanel_new(scr, st->header, st->host);
   ScreenManager_run(scr, NULL, NULL, "Setup");
   ScreenManager_delete(scr);
   if (settings->changed) {
      CRT_setMouse(settings->enableMouse);
      Header_writeBackToSettings(st->header);
   }
}

static bool changePriority(MainPanel* panel, int delta) {
   bool anyTagged;
   bool ok = MainPanel_foreachRow(panel, Process_rowChangePriorityBy, (Arg) { .i = delta }, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static void addUserToVector(ht_key_t key, void* userCast, void* panelCast) {
   const char* user = userCast;
   Panel* panel = panelCast;
   Panel_add(panel, (Object*) ListItem_new(user, key));
}

bool Action_setUserOnly(const char* userName, uid_t* userId) {
   const struct passwd* user = getpwnam(userName);
   if (user) {
      *userId = user->pw_uid;
      return true;
   }
   *userId = (uid_t)-1;
   return false;
}

static void tagAllChildren(Panel* panel, Row* parent) {
   parent->tag = true;
   int parent_id = parent->id;
   for (int i = 0; i < Panel_size(panel); i++) {
      Row* row = (Row*) Panel_get(panel, i);
      if (!row->tag && Row_isChildOf(row, parent_id)) {
         tagAllChildren(panel, row);
      }
   }
}

static bool expandCollapse(Panel* panel) {
   Row* row = (Row*) Panel_getSelected(panel);
   if (!row)
      return false;

   row->showChildren = !row->showChildren;
   return true;
}

static bool collapseIntoParent(Panel* panel) {
   const Row* r = (Row*) Panel_getSelected(panel);
   if (!r)
      return false;

   int parent_id = Row_getGroupOrParent(r);
   for (int i = 0; i < Panel_size(panel); i++) {
      Row* row = (Row*) Panel_get(panel, i);
      if (row->id == parent_id) {
         row->showChildren = false;
         Panel_setSelected(panel, i);
         return true;
      }
   }
   return false;
}

Htop_Reaction Action_setSortKey(Settings* settings, ProcessField sortKey) {
   ScreenSettings_setSortKey(settings->ss, (RowField) sortKey);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR | HTOP_KEEP_FOLLOWING;
}

// ----------------------------------------

static bool Action_writeableProcess(State* st) {
   const Settings* settings = st->host->settings;
   bool readonly = Settings_isReadonly() || settings->ss->dynamic;
   return !readonly;
}

static bool Action_readableProcess(State* st) {
   const Settings* settings = st->host->settings;
   return !settings->ss->dynamic;
}

static Htop_Reaction actionSetSortColumn(State* st) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Sort   ", "Cancel "));
   Panel_setHeader(sortPanel, "Sort by");
   Machine* host = st->host;
   Settings* settings = host->settings;
   const RowField* fields = settings->ss->fields;
   Hashtable* dynamicColumns = settings->dynamicColumns;
   for (int i = 0; fields[i]; i++) {
      char* name = NULL;
      if (fields[i] >= ROW_DYNAMIC_FIELDS) {
         DynamicColumn* column = Hashtable_get(dynamicColumns, fields[i]);
         if (!column)
            continue;
         name = xStrdup(column->caption ? column->caption : column->name);
      } else {
         name = String_trim(Process_fields[fields[i]].name);
      }
      Panel_add(sortPanel, (Object*) ListItem_new(name, fields[i]));
      if (fields[i] == ScreenSettings_getActiveSortKey(settings->ss))
         Panel_setSelected(sortPanel, i);

      free(name);
   }
   const ListItem* field = (const ListItem*) Action_pickFromVector(st, sortPanel, 14, false);
   if (field) {
      reaction |= Action_setSortKey(settings, field->key);
   }
   Object_delete(sortPanel);

   host->activeTable->needsSort = true;

   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionSortByPID(State* st) {
   return Action_setSortKey(st->host->settings, PID);
}

static Htop_Reaction actionSortByMemory(State* st) {
   return Action_setSortKey(st->host->settings, PERCENT_MEM);
}

static Htop_Reaction actionSortByCPU(State* st) {
   return Action_setSortKey(st->host->settings, PERCENT_CPU);
}

static Htop_Reaction actionSortByTime(State* st) {
   return Action_setSortKey(st->host->settings, TIME);
}

static Htop_Reaction actionToggleKernelThreads(State* st) {
   Settings* settings = st->host->settings;
   settings->hideKernelThreads = !settings->hideKernelThreads;
   settings->lastUpdate++;

   Machine_scanTables(st->host); // needed to not have a visible delay showing wrong data

   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleUserlandThreads(State* st) {
   Settings* settings = st->host->settings;
   settings->hideUserlandThreads = !settings->hideUserlandThreads;
   settings->lastUpdate++;

   Machine_scanTables(st->host); // needed to not have a visible delay showing wrong data

   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleRunningInContainer(State* st) {
   Settings* settings = st->host->settings;
   settings->hideRunningInContainer = !settings->hideRunningInContainer;
   settings->lastUpdate++;

   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleProgramPath(State* st) {
   Settings* settings = st->host->settings;
   settings->showProgramPath = !settings->showProgramPath;
   settings->lastUpdate++;

   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleMergedCommand(State* st) {
   Settings* settings = st->host->settings;
   settings->showMergedCommand = !settings->showMergedCommand;
   settings->lastUpdate++;

   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionToggleTreeView(State* st) {
   Machine* host = st->host;
   ScreenSettings* ss = host->settings->ss;
   ss->treeView = !ss->treeView;

   if (!ss->allBranchesCollapsed)
      Table_expandTree(host->activeTable);

   host->activeTable->needsSort = true;

   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionToggleHideMeters(State* st) {
   st->hideMeters = !st->hideMeters;
   return HTOP_RESIZE | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionExpandOrCollapseAllBranches(State* st) {
   Machine* host = st->host;
   ScreenSettings* ss = host->settings->ss;
   if (!ss->treeView) {
      return HTOP_OK;
   }
   ss->allBranchesCollapsed = !ss->allBranchesCollapsed;
   if (ss->allBranchesCollapsed)
      Table_collapseAllBranches(host->activeTable);
   else
      Table_expandTree(host->activeTable);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionIncFilter(State* st) {
   Machine* host = st->host;
   IncSet* inc = (st->mainPanel)->inc;
   IncSet_activate(inc, INC_FILTER, (Panel*)st->mainPanel);
   host->activeTable->incFilter = IncSet_filter(inc);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(State* st) {
   IncSet_reset(st->mainPanel->inc, INC_SEARCH);
   IncSet_activate(st->mainPanel->inc, INC_SEARCH, (Panel*)st->mainPanel);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionHigherPriority(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   bool changed = changePriority(st->mainPanel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   bool changed = changePriority(st->mainPanel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(State* st) {
   Machine* host = st->host;
   ScreenSettings_invertSortOrder(host->settings->ss);
   host->activeTable->needsSort = true;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionExpandOrCollapse(State* st) {
   if (!st->host->settings->ss->treeView)
      return HTOP_OK;

   bool changed = expandCollapse((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionCollapseIntoParent(State* st) {
   if (!st->host->settings->ss->treeView) {
      return HTOP_OK;
   }
   bool changed = collapseIntoParent((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(State* st) {
   return st->host->settings->ss->treeView ? actionExpandOrCollapse(st) : actionSetSortColumn(st);
}

static inline void setActiveScreen(Settings* settings, State* st, unsigned int ssIdx) {
   assert(settings->ssIndex == ssIdx);
   Machine* host = st->host;

   settings->ss = settings->screens[ssIdx];
   if (!settings->ss->table)
      settings->ss->table = host->processTable;
   host->activeTable = settings->ss->table;

   // set correct functionBar - readonly if requested, and/or with non-process screens
   bool readonly = Settings_isReadonly() || (host->activeTable != host->processTable);
   MainPanel_setFunctionBar(st->mainPanel, readonly);
}

static Htop_Reaction actionNextScreen(State* st) {
   Settings* settings = st->host->settings;
   settings->ssIndex++;
   if (settings->ssIndex == settings->nScreens) {
      settings->ssIndex = 0;
   }
   setActiveScreen(settings, st, settings->ssIndex);
   return HTOP_UPDATE_PANELHDR | HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionPrevScreen(State* st) {
   Settings* settings = st->host->settings;
   if (settings->ssIndex == 0) {
      settings->ssIndex = settings->nScreens - 1;
   } else {
      settings->ssIndex--;
   }
   setActiveScreen(settings, st, settings->ssIndex);
   return HTOP_UPDATE_PANELHDR | HTOP_REFRESH | HTOP_REDRAW_BAR;
}

Htop_Reaction Action_setScreenTab(State* st, int x) {
   Settings* settings = st->host->settings;
   const int bracketWidth = (int)strlen("[]");

   if (x < SCREEN_TAB_MARGIN_LEFT) {
      return 0;
   }

   int rem = x - SCREEN_TAB_MARGIN_LEFT;
   for (unsigned int i = 0; i < settings->nScreens; i++) {
      const char* tab = settings->screens[i]->heading;
      int width = rem >= bracketWidth ? (int)strnlen(tab, rem - bracketWidth + 1) : 0;
      if (width >= rem - bracketWidth + 1) {
         settings->ssIndex = i;
         setActiveScreen(settings, st, i);
         return HTOP_UPDATE_PANELHDR | HTOP_REFRESH | HTOP_REDRAW_BAR;
      }

      rem -= bracketWidth + width;
      if (rem < SCREEN_TAB_COLUMN_GAP) {
         return 0;
      }

      rem -= SCREEN_TAB_COLUMN_GAP;
   }
   return 0;
}

static Htop_Reaction actionQuit(ATTR_UNUSED State* st) {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   Machine* host = st->host;
   if (host->activeCPUs == 1)
      return HTOP_OK;

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
   const Row* row = (const Row*) Panel_getSelected((Panel*)st->mainPanel);
   if (!row)
      return HTOP_OK;

   Affinity* affinity1 = Affinity_rowGet(row, host);
   if (!affinity1)
      return HTOP_OK;

   int width;
   Panel* affinityPanel = AffinityPanel_new(host, affinity1, &width);
   Affinity_delete(affinity1);

   const void* set = Action_pickFromVector(st, affinityPanel, width, true);
   if (set) {
      Affinity* affinity2 = AffinityPanel_getAffinity(affinityPanel, host);
      bool ok = MainPanel_foreachRow(st->mainPanel, Affinity_rowSet, (Arg) { .v = affinity2 }, NULL);
      if (!ok)
         beep();
      Affinity_delete(affinity2);
   }
   Object_delete(affinityPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
#else
   return HTOP_OK;
#endif
}

#ifdef SCHEDULER_SUPPORT
static Htop_Reaction actionSetSchedPolicy(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_KEEP_FOLLOWING;

   static int preSelectedPolicy = SCHEDULINGPANEL_INITSELECTEDPOLICY;
   static int preSelectedPriority = SCHEDULINGPANEL_INITSELECTEDPRIORITY;

   Panel* schedPanel = Scheduling_newPolicyPanel(preSelectedPolicy);

   const ListItem* policy;
   for (;;) {
      policy = (const ListItem*) Action_pickFromVector(st, schedPanel, 18, true);

      if (!policy || policy->key != -1)
         break;

      Scheduling_togglePolicyPanelResetOnFork(schedPanel);
   }

   if (policy) {
      preSelectedPolicy = policy->key;

      Panel* prioPanel = Scheduling_newPriorityPanel(policy->key, preSelectedPriority);
      if (prioPanel) {
         const ListItem* prio = (const ListItem*) Action_pickFromVector(st, prioPanel, 14, true);
         if (prio)
            preSelectedPriority = prio->key;

         Panel_delete((Object*) prioPanel);
      }

      SchedulingArg v = { .policy = preSelectedPolicy, .priority = preSelectedPriority };

      bool ok = MainPanel_foreachRow(st->mainPanel, Scheduling_rowSetPolicy, (Arg) { .v = &v }, NULL);
      if (!ok)
         beep();
   }

   Panel_delete((Object*)schedPanel);

   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_KEEP_FOLLOWING;
}
#endif  /* SCHEDULER_SUPPORT */

static Htop_Reaction actionKill(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   static int preSelectedSignal = SIGNALSPANEL_INITSELECTEDSIGNAL;

   Panel* signalsPanel = SignalsPanel_new(preSelectedSignal);
   const ListItem* sgn = (ListItem*) Action_pickFromVector(st, signalsPanel, 14, true);
   if (sgn && sgn->key != 0) {
      preSelectedSignal = sgn->key;
      Panel_setHeader((Panel*)st->mainPanel, "Sending...");
      Panel_draw((Panel*)st->mainPanel, false, true, true, State_hideFunctionBar(st));
      refresh();
      bool ok = MainPanel_foreachRow(st->mainPanel, Process_rowSendSignal, (Arg) { .i = sgn->key }, NULL);
      if (!ok) {
         beep();
      }
      napms(500);
   }
   Panel_delete((Object*)signalsPanel);

   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(State* st) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Show   ", "Cancel "));
   Panel_setHeader(usersPanel, "Show processes of:");
   Machine* host = st->host;
   UsersTable_foreach(host->usersTable, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("All users", -1);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   const ListItem* picked = (ListItem*) Action_pickFromVector(st, usersPanel, 19, false);
   if (picked) {
      if (picked == allUsers) {
         host->userId = (uid_t)-1;
      } else {
         Action_setUserOnly(ListItem_getRef(picked), &host->userId);
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

Htop_Reaction Action_follow(State* st) {
   st->host->activeTable->following = MainPanel_selectedRow(st->mainPanel);
   Panel_setSelectionColor((Panel*)st->mainPanel, PANEL_SELECTION_FOLLOW);
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(State* st) {
   Action_runSetup(st);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR | HTOP_RESIZE;
}

static Htop_Reaction actionLsof(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));

   OpenFilesScreen* ofs = OpenFilesScreen_new(p);
   InfoScreen_run((InfoScreen*)ofs);
   OpenFilesScreen_delete((Object*)ofs);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowLocks(State* st) {
   if (!Action_readableProcess(st))
      return HTOP_OK;

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));

   ProcessLocksScreen* pls = ProcessLocksScreen_new(p);
   InfoScreen_run((InfoScreen*)pls);
   ProcessLocksScreen_delete((Object*)pls);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(State* st) {
   if (!Action_writeableProcess(st))
      return HTOP_OK;

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));

   TraceScreen* ts = TraceScreen_new(p);
   bool ok = TraceScreen_forkTracer(ts);
   if (ok) {
      InfoScreen_run((InfoScreen*)ts);
   }
   TraceScreen_delete((Object*)ts);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTag(State* st) {
   Row* r = (Row*) Panel_getSelected((Panel*)st->mainPanel);
   if (!r)
      return HTOP_OK;

   Row_toggleTag(r);
   Panel_onKey((Panel*)st->mainPanel, KEY_DOWN);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw(ATTR_UNUSED State* st) {
   clear();
   // HTOP_RECALCULATE here to make Ctrl-L also refresh the data and not only redraw
   return HTOP_RECALCULATE | HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTogglePauseUpdate(State* st) {
   st->pauseUpdate = !st->pauseUpdate;
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_KEEP_FOLLOWING;
}

static const struct {
   const char* key;
   bool roInactive;
   const char* info;
} helpLeft[] = {
   { .key = "      #: ",  .roInactive = false, .info = "hide/show header meters" },
   { .key = "    Tab: ",  .roInactive = false, .info = "switch to next screen tab" },
   { .key = " Arrows: ",  .roInactive = false, .info = "scroll process list" },
   { .key = " Digits: ",  .roInactive = false, .info = "incremental PID search" },
   { .key = "   F3 /: ",  .roInactive = false, .info = "incremental name search" },
   { .key = "   F4 \\: ", .roInactive = false, .info = "incremental name filtering" },
   { .key = "   F5 t: ",  .roInactive = false, .info = "tree view" },
   { .key = "      p: ",  .roInactive = false, .info = "toggle program path" },
   { .key = "      m: ",  .roInactive = false, .info = "toggle merged command" },
   { .key = "      Z: ",  .roInactive = false, .info = "pause/resume process updates" },
   { .key = "      u: ",  .roInactive = false, .info = "show processes of a single user" },
   { .key = "      H: ",  .roInactive = false, .info = "hide/show user process threads" },
   { .key = "      K: ",  .roInactive = false, .info = "hide/show kernel threads" },
   { .key = "      O: ",  .roInactive = false, .info = "hide/show processes in containers" },
   { .key = "      F: ",  .roInactive = false, .info = "cursor follows process" },
   { .key = "  + - *: ",  .roInactive = false, .info = "expand/collapse tree/toggle all" },
   { .key = "N P M T: ",  .roInactive = false, .info = "sort by PID, CPU%, MEM% or TIME" },
   { .key = "      I: ",  .roInactive = false, .info = "invert sort order" },
   { .key = " F6 > .: ",  .roInactive = false, .info = "select sort column" },
   { .key = NULL, .info = NULL }
};

static const struct {
   const char* key;
   bool roInactive;
   const char* info;
} helpRight[] = {
   { .key = "  S-Tab: ", .roInactive = false, .info = "switch to previous screen tab" },
   { .key = "  Space: ", .roInactive = false, .info = "tag process" },
   { .key = "      c: ", .roInactive = false, .info = "tag process and its children" },
   { .key = "      U: ", .roInactive = false, .info = "untag all processes" },
   { .key = "   F9 k: ", .roInactive = true,  .info = "kill process/tagged processes" },
   { .key = "   F7 ]: ", .roInactive = true,  .info = "higher priority (root only)" },
   { .key = "   F8 [: ", .roInactive = true,  .info = "lower priority (+ nice)" },
#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
   { .key = "      a: ", .roInactive = true, .info = "set CPU affinity" },
#endif
   { .key = "      e: ", .roInactive = false, .info = "show process environment" },
   { .key = "      i: ", .roInactive = true,  .info = "set IO priority" },
   { .key = "      l: ", .roInactive = true,  .info = "list open files with lsof" },
   { .key = "      x: ", .roInactive = false, .info = "list file locks of process" },
   { .key = "      s: ", .roInactive = true,  .info = "trace syscalls with strace" },
   { .key = "      w: ", .roInactive = false, .info = "wrap process command in multiple lines" },
#ifdef SCHEDULER_SUPPORT
   { .key = "      Y: ", .roInactive = true,  .info = "set scheduling policy" },
#endif
   { .key = " F2 C S: ", .roInactive = false, .info = "setup" },
   { .key = " F1 h ?: ", .roInactive = false, .info = "show this help screen" },
   { .key = "  F10 q: ", .roInactive = false, .info = "quit" },
   { .key = NULL, .info = NULL }
};

static inline void addattrstr( int attr, const char* str) {
   attrset(attr);
   addstr(str);
}

static Htop_Reaction actionHelp(State* st) {
   clear();
   attrset(CRT_colors[HELP_BOLD]);

   for (int i = 0; i < LINES - 1; i++)
      mvhline(i, 0, ' ', COLS);

   int line = 0;

   mvaddstr(line++, 0, "htop " VERSION " - " COPYRIGHT);
   mvaddstr(line++, 0, "Released under the GNU GPLv2+. See 'man' page for more info.");

   attrset(CRT_colors[DEFAULT_COLOR]);
   line++;
   mvaddstr(line++, 0, "CPU usage bar: ");

#define addbartext(attr, prefix, text)               \
   do {                                              \
      addattrstr(CRT_colors[DEFAULT_COLOR], prefix); \
      addattrstr(attr, text);                        \
   } while(0)

   addattrstr(CRT_colors[BAR_BORDER], "[");
   addbartext(CRT_colors[CPU_NICE_TEXT], "", "low");
   addbartext(CRT_colors[CPU_NORMAL], "/", "normal");
   addbartext(CRT_colors[CPU_SYSTEM], "/", "kernel");
   if (st->host->settings->detailedCPUTime) {
      addbartext(CRT_colors[CPU_IRQ], "/", "irq");
      addbartext(CRT_colors[CPU_SOFTIRQ], "/", "soft-irq");
      addbartext(CRT_colors[CPU_STEAL], "/", "steal");
      addbartext(CRT_colors[CPU_GUEST], "/", "guest");
      addbartext(CRT_colors[CPU_IOWAIT], "/", "io-wait");
      addbartext(CRT_colors[BAR_SHADOW], " ", "used%");
   } else {
      addbartext(CRT_colors[CPU_GUEST], "/", "guest");
      addbartext(CRT_colors[BAR_SHADOW], "                            ", "used%");
   }
   addattrstr(CRT_colors[BAR_BORDER], "]");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Memory bar:    ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   // memory classes are OS-specific and provided in their <os>/Platform.c implementation
   // ideal length of memory bar == 56 chars. Any length < 45 requires padding to 45.
   // [0        1         2         3         4         5      ]
   // [12345678901234567890123456789012345678901234567890123456]
   // [                                            ^    5      ]
   // [class1/class2/class3/.../classN               used/total]
   int barTxtLen = 0;
   for (unsigned int i = 0; i < Platform_numberOfMemoryClasses; i++) {
      if (!st->host->settings->showCachedMemory && Platform_memoryClasses[i].countsAsCache)
         continue; // skip reclaimable cache memory classes if "show cached memory" is not ticked
      addbartext(CRT_colors[Platform_memoryClasses[i].color], (i == 0 ? "" : "/"), Platform_memoryClasses[i].label);
      barTxtLen += (i == 0 ? 0 : 1) + strlen (Platform_memoryClasses[i].label);
   }
   for (int i = barTxtLen; i < 45; i++)
      addattrstr(CRT_colors[BAR_SHADOW], " "); // pad to 45 chars if necessary
   addbartext(CRT_colors[BAR_SHADOW], " ", "used");
   addbartext(CRT_colors[BAR_SHADOW], "/", "total");
   addattrstr(CRT_colors[BAR_BORDER], "]");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Swap bar:      ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addbartext(CRT_colors[SWAP], "", "used");
#ifdef HTOP_LINUX
   addbartext(CRT_colors[SWAP_CACHE], "/", "cache");
   addbartext(CRT_colors[SWAP_FRONTSWAP], "/", "frontswap");
#else
   addbartext(CRT_colors[BAR_SHADOW], "                ", "");
#endif
   addbartext(CRT_colors[BAR_SHADOW], "                          ", "used");
   addbartext(CRT_colors[BAR_SHADOW], "/", "total");
   addattrstr(CRT_colors[BAR_BORDER], "]");

   line++;

#undef addbartext

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Type and layout of header meters are configurable in the setup screen.");
   if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
      mvaddstr(line, 0, "In monochrome, meters display as different chars, in order: |#*@$%&.");
   }
   line++;

#define addattrstatestr(attr, state, desc)              \
   do {                                                 \
      addattrstr(attr, state);                          \
      addattrstr(CRT_colors[DEFAULT_COLOR], ": " desc); \
   } while(0)

   mvaddstr(line, 0, "Process state: ");
   addattrstatestr(CRT_colors[PROCESS_RUN_STATE], "R", "running; ");
   addattrstatestr(CRT_colors[PROCESS_SHADOW], "S", "sleeping; ");
   addattrstatestr(CRT_colors[PROCESS_RUN_STATE], "t", "traced/stopped; ");
   addattrstatestr(CRT_colors[PROCESS_D_STATE], "Z", "zombie; ");
   addattrstatestr(CRT_colors[PROCESS_D_STATE], "D", "disk sleep");
   attrset(CRT_colors[DEFAULT_COLOR]);

#undef addattrstatestr

   line += 2;

   const bool readonly = Settings_isReadonly();

   int item;
   for (item = 0; helpLeft[item].key; item++) {
      attrset((helpLeft[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[DEFAULT_COLOR]);
      mvaddstr(line + item, 10, helpLeft[item].info);
      attrset((helpLeft[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[HELP_BOLD]);
      mvaddstr(line + item, 1,  helpLeft[item].key);
      if (String_eq(helpLeft[item].key, "      H: ")) {
         attrset((helpLeft[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[PROCESS_THREAD]);
         mvaddstr(line + item, 33, "threads");
      } else if (String_eq(helpLeft[item].key, "      K: ")) {
         attrset((helpLeft[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[PROCESS_THREAD]);
         mvaddstr(line + item, 27, "threads");
      }
   }
   int leftHelpItems = item;

   for (item = 0; helpRight[item].key; item++) {
      attrset((helpRight[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[HELP_BOLD]);
      mvaddstr(line + item, 43, helpRight[item].key);
      attrset((helpRight[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[DEFAULT_COLOR]);
      mvaddstr(line + item, 52, helpRight[item].info);
   }
   line += MAXIMUM(leftHelpItems, item);
   line++;

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(line++, 0, "Press any key to return.");
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();

   return HTOP_RECALCULATE | HTOP_REDRAW_BAR | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionUntagAll(State* st) {
   for (int i = 0; i < Panel_size((Panel*)st->mainPanel); i++) {
      Row* row = (Row*) Panel_get((Panel*)st->mainPanel, i);
      row->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(State* st) {
   Row* row = (Row*) Panel_getSelected((Panel*)st->mainPanel);
   if (!row)
      return HTOP_OK;

   tagAllChildren((Panel*)st->mainPanel, row);
   return HTOP_OK;
}

static Htop_Reaction actionShowEnvScreen(State* st) {
   if (!Action_readableProcess(st))
      return HTOP_OK;

   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));

   EnvScreen* es = EnvScreen_new(p);
   InfoScreen_run((InfoScreen*)es);
   EnvScreen_delete((Object*)es);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowCommandScreen(State* st) {
   if (!Action_readableProcess(st))
      return HTOP_OK;

   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));

   CommandScreen* cmdScr = CommandScreen_new(p);
   InfoScreen_run((InfoScreen*)cmdScr);
   CommandScreen_delete((Object*)cmdScr);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

void Action_setBindings(Htop_Action* keys) {
   keys[' '] = actionTag;
   keys['#'] = actionToggleHideMeters;
   keys['*'] = actionExpandOrCollapseAllBranches;
   keys['+'] = actionExpandOrCollapse;
   keys[','] = actionSetSortColumn;
   keys['-'] = actionExpandOrCollapse;
   keys['.'] = actionSetSortColumn;
   keys['/'] = actionIncSearch;
   keys['<'] = actionSetSortColumn;
   keys['='] = actionExpandOrCollapse;
   keys['>'] = actionSetSortColumn;
   keys['?'] = actionHelp;
   keys['C'] = actionSetup;
   keys['F'] = Action_follow;
   keys['H'] = actionToggleUserlandThreads;
   keys['I'] = actionInvertSortOrder;
   keys['K'] = actionToggleKernelThreads;
   keys['M'] = actionSortByMemory;
   keys['N'] = actionSortByPID;
   keys['O'] = actionToggleRunningInContainer;
   keys['P'] = actionSortByCPU;
   keys['S'] = actionSetup;
   keys['T'] = actionSortByTime;
   keys['U'] = actionUntagAll;
#ifdef SCHEDULER_SUPPORT
   keys['Y'] = actionSetSchedPolicy;
#endif
   keys['Z'] = actionTogglePauseUpdate;
   keys['['] = actionLowerPriority;
   keys['\014'] = actionRedraw; // Ctrl+L
   keys['\177'] = actionCollapseIntoParent;
   keys['\\'] = actionIncFilter;
   keys[']'] = actionHigherPriority;
   keys['a'] = actionSetAffinity;
   keys['c'] = actionTagAllChildren;
   keys['e'] = actionShowEnvScreen;
   keys['h'] = actionHelp;
   keys['k'] = actionKill;
   keys['l'] = actionLsof;
   keys['m'] = actionToggleMergedCommand;
   keys['p'] = actionToggleProgramPath;
   keys['q'] = actionQuit;
   keys['s'] = actionStrace;
   keys['t'] = actionToggleTreeView;
   keys['u'] = actionFilterByUser;
   keys['w'] = actionShowCommandScreen;
   keys['x'] = actionShowLocks;
   keys[KEY_F(1)] = actionHelp;
   keys[KEY_F(2)] = actionSetup;
   keys[KEY_F(3)] = actionIncSearch;
   keys[KEY_F(4)] = actionIncFilter;
   keys[KEY_F(5)] = actionToggleTreeView;
   keys[KEY_F(6)] = actionSetSortColumn;
   keys[KEY_F(7)] = actionHigherPriority;
   keys[KEY_F(8)] = actionLowerPriority;
   keys[KEY_F(9)] = actionKill;
   keys[KEY_F(10)] = actionQuit;
   keys[KEY_F(18)] = actionExpandCollapseOrSortColumn;
   keys[KEY_RECLICK] = actionExpandOrCollapse;
   keys[KEY_SHIFT_TAB] = actionPrevScreen;
   keys['\t'] = actionNextScreen;
}
