/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Action.h"

#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>

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
#include "OpenFilesScreen.h"
#include "Process.h"
#include "ProcessLocksScreen.h"
#include "ProvideCurses.h"
#include "ScreenManager.h"
#include "SignalsPanel.h"
#include "TraceScreen.h"
#include "Vector.h"
#include "XUtils.h"

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
#include "Affinity.h"
#include "AffinityPanel.h"
#endif


Object* Action_pickFromVector(State* st, Panel* list, int x, bool followProcess) {
   MainPanel* mainPanel = st->mainPanel;
   Header* header = st->header;

   int y = ((Panel*)mainPanel)->y;
   ScreenManager* scr = ScreenManager_new(header, st->settings, st, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, x);
   ScreenManager_add(scr, (Panel*)mainPanel, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   int pid = followProcess ? MainPanel_selectedPid(mainPanel) : -1;
   if (followProcess && header->pl->following == -1) {
      header->pl->following = pid;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch, NULL);
   if (unfollow) {
      header->pl->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move((Panel*)mainPanel, 0, y);
   Panel_resize((Panel*)mainPanel, COLS, LINES - y - 1);
   if (panelFocus == list && ch == 13) {
      if (followProcess) {
         const Process* selected = (const Process*)Panel_getSelected((Panel*)mainPanel);
         if (selected && selected->pid == pid)
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
   ScreenManager* scr = ScreenManager_new(st->header, st->settings, st, true);
   CategoriesPanel_new(scr, st->settings, st->header, st->pl);
   ScreenManager_run(scr, NULL, NULL, "Setup");
   ScreenManager_delete(scr);
   if (st->settings->changed) {
      CRT_setMouse(st->settings->enableMouse);
      Header_writeBackToSettings(st->header);
   }
}

static bool changePriority(MainPanel* panel, int delta) {
   bool anyTagged;
   bool ok = MainPanel_foreachProcess(panel, Process_changePriorityBy, (Arg) { .i = delta }, &anyTagged);
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

static void tagAllChildren(Panel* panel, Process* parent) {
   parent->tag = true;
   pid_t ppid = parent->pid;
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      if (!p->tag && Process_isChildOf(p, ppid)) {
         tagAllChildren(panel, p);
      }
   }
}

static bool expandCollapse(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p)
      return false;

   p->showChildren = !p->showChildren;
   return true;
}

static bool collapseIntoParent(Panel* panel) {
   const Process* p = (Process*) Panel_getSelected(panel);
   if (!p)
      return false;

   pid_t ppid = Process_getParentPid(p);
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* q = (Process*) Panel_get(panel, i);
      if (q->pid == ppid) {
         q->showChildren = false;
         Panel_setSelected(panel, i);
         return true;
      }
   }
   return false;
}

Htop_Reaction Action_setSortKey(Settings* settings, ProcessField sortKey) {
   ScreenSettings_setSortKey(settings->ss, sortKey);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR | HTOP_KEEP_FOLLOWING;
}

// ----------------------------------------

static Htop_Reaction actionSetSortColumn(State* st) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Sort   ", "Cancel "));
   Panel_setHeader(sortPanel, "Sort by");
   const Settings* settings = st->settings;
   const ProcessField* fields = settings->ss->fields;
   Hashtable* dynamicColumns = settings->dynamicColumns;
   for (int i = 0; fields[i]; i++) {
      char* name = NULL;
      if (fields[i] >= LAST_PROCESSFIELD) {
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
      reaction |= Action_setSortKey(st->settings, field->key);
   }
   Object_delete(sortPanel);

   st->pl->needsSort = true;

   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionSortByPID(State* st) {
   return Action_setSortKey(st->settings, PID);
}

static Htop_Reaction actionSortByMemory(State* st) {
   return Action_setSortKey(st->settings, PERCENT_MEM);
}

static Htop_Reaction actionSortByCPU(State* st) {
   return Action_setSortKey(st->settings, PERCENT_CPU);
}

static Htop_Reaction actionSortByTime(State* st) {
   return Action_setSortKey(st->settings, TIME);
}

static Htop_Reaction actionToggleKernelThreads(State* st) {
   st->settings->hideKernelThreads = !st->settings->hideKernelThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleUserlandThreads(State* st) {
   st->settings->hideUserlandThreads = !st->settings->hideUserlandThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionToggleProgramPath(State* st) {
   st->settings->showProgramPath = !st->settings->showProgramPath;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleMergedCommand(State* st) {
   st->settings->showMergedCommand = !st->settings->showMergedCommand;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleTreeView(State* st) {
   ScreenSettings* ss = st->settings->ss;
   ss->treeView = !ss->treeView;

   if (!ss->allBranchesCollapsed)
      ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionExpandOrCollapseAllBranches(State* st) {
   ScreenSettings* ss = st->settings->ss;
   if (!ss->treeView) {
      return HTOP_OK;
   }
   ss->allBranchesCollapsed = !ss->allBranchesCollapsed;
   if (ss->allBranchesCollapsed)
      ProcessList_collapseAllBranches(st->pl);
   else
      ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionIncFilter(State* st) {
   IncSet* inc = (st->mainPanel)->inc;
   IncSet_activate(inc, INC_FILTER, (Panel*)st->mainPanel);
   st->pl->incFilter = IncSet_filter(inc);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(State* st) {
   IncSet_reset(st->mainPanel->inc, INC_SEARCH);
   IncSet_activate(st->mainPanel->inc, INC_SEARCH, (Panel*)st->mainPanel);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionHigherPriority(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   bool changed = changePriority(st->mainPanel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   bool changed = changePriority(st->mainPanel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(State* st) {
   ScreenSettings_invertSortOrder(st->settings->ss);
   st->pl->needsSort = true;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionExpandOrCollapse(State* st) {
   bool changed = expandCollapse((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionCollapseIntoParent(State* st) {
   if (!st->settings->ss->treeView) {
      return HTOP_OK;
   }
   bool changed = collapseIntoParent((Panel*)st->mainPanel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(State* st) {
   return st->settings->ss->treeView ? actionExpandOrCollapse(st) : actionSetSortColumn(st);
}

static Htop_Reaction actionNextScreen(State* st) {
   Settings* settings = st->settings;
   settings->ssIndex++;
   if (settings->ssIndex == settings->nScreens) {
      settings->ssIndex = 0;
   }
   settings->ss = settings->screens[settings->ssIndex];
   return HTOP_REFRESH;
}

static Htop_Reaction actionPrevScreen(State* st) {
   Settings* settings = st->settings;
   if (settings->ssIndex == 0) {
      settings->ssIndex = settings->nScreens - 1;
   } else {
      settings->ssIndex--;
   }
   settings->ss = settings->screens[settings->ssIndex];
   return HTOP_REFRESH;
}

Htop_Reaction Action_setScreenTab(Settings* settings, int x) {
   int s = 2;
   for (unsigned int i = 0; i < settings->nScreens; i++) {
      if (x < s) {
         return 0;
      }
      const char* name = settings->screens[i]->name;
      int len = strlen(name);
      if (x <= s + len + 1) {
         settings->ssIndex = i;
         settings->ss = settings->screens[i];
         return HTOP_REFRESH;
      }
      s += len + 3;
   }
   return 0;
}

static Htop_Reaction actionQuit(ATTR_UNUSED State* st) {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   if (st->pl->activeCPUs == 1)
      return HTOP_OK;

#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
   const Process* p = (const Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   Affinity* affinity1 = Affinity_get(p, st->pl);
   if (!affinity1)
      return HTOP_OK;

   int width;
   Panel* affinityPanel = AffinityPanel_new(st->pl, affinity1, &width);
   Affinity_delete(affinity1);

   const void* set = Action_pickFromVector(st, affinityPanel, width, true);
   if (set) {
      Affinity* affinity2 = AffinityPanel_getAffinity(affinityPanel, st->pl);
      bool ok = MainPanel_foreachProcess(st->mainPanel, Affinity_set, (Arg) { .v = affinity2 }, NULL);
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

static Htop_Reaction actionKill(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   static int preSelectedSignal = SIGNALSPANEL_INITSELECTEDSIGNAL;

   Panel* signalsPanel = SignalsPanel_new(preSelectedSignal);
   const ListItem* sgn = (ListItem*) Action_pickFromVector(st, signalsPanel, 14, true);
   if (sgn && sgn->key != 0) {
      preSelectedSignal = sgn->key;
      Panel_setHeader((Panel*)st->mainPanel, "Sending...");
      Panel_draw((Panel*)st->mainPanel, false, true, true, State_hideFunctionBar(st));
      refresh();
      MainPanel_foreachProcess(st->mainPanel, Process_sendSignal, (Arg) { .i = sgn->key }, NULL);
      napms(500);
   }
   Panel_delete((Object*)signalsPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(State* st) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Show   ", "Cancel "));
   Panel_setHeader(usersPanel, "Show processes of:");
   UsersTable_foreach(st->ut, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("All users", -1);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   const ListItem* picked = (ListItem*) Action_pickFromVector(st, usersPanel, 19, false);
   if (picked) {
      if (picked == allUsers) {
         st->pl->userId = (uid_t)-1;
      } else {
         Action_setUserOnly(ListItem_getRef(picked), &(st->pl->userId));
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

Htop_Reaction Action_follow(State* st) {
   st->pl->following = MainPanel_selectedPid(st->mainPanel);
   Panel_setSelectionColor((Panel*)st->mainPanel, PANEL_SELECTION_FOLLOW);
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(State* st) {
   Action_runSetup(st);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR | HTOP_RESIZE;
}

static Htop_Reaction actionLsof(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   OpenFilesScreen* ofs = OpenFilesScreen_new(p);
   InfoScreen_run((InfoScreen*)ofs);
   OpenFilesScreen_delete((Object*)ofs);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowLocks(State* st) {
   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;
   ProcessLocksScreen* pls = ProcessLocksScreen_new(p);
   InfoScreen_run((InfoScreen*)pls);
   ProcessLocksScreen_delete((Object*)pls);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

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
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   Process_toggleTag(p);
   Panel_onKey((Panel*)st->mainPanel, KEY_DOWN);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw(ATTR_UNUSED State* st) {
   clear();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTogglePauseProcessUpdate(State* st) {
   st->pauseProcessUpdate = !st->pauseProcessUpdate;
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static const struct {
   const char* key;
   bool roInactive;
   const char* info;
} helpLeft[] = {
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
   if (st->settings->detailedCPUTime) {
      addbartext(CRT_colors[CPU_IRQ], "/", "irq");
      addbartext(CRT_colors[CPU_SOFTIRQ], "/", "soft-irq");
      addbartext(CRT_colors[CPU_STEAL], "/", "steal");
      addbartext(CRT_colors[CPU_GUEST], "/", "guest");
      addbartext(CRT_colors[CPU_IOWAIT], "/", "io-wait");
      addbartext(CRT_colors[BAR_SHADOW], " ", "used%");
   } else {
      addbartext(CRT_colors[CPU_GUEST], "/", "guest");
      addbartext(CRT_colors[BAR_SHADOW], "                  ", "used%");
   }
   addattrstr(CRT_colors[BAR_BORDER], "]");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Memory bar:    ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addbartext(CRT_colors[MEMORY_USED], "", "used");
   addbartext(CRT_colors[MEMORY_BUFFERS_TEXT], "/", "buffers");
   addbartext(CRT_colors[MEMORY_SHARED], "/", "shared");
   addbartext(CRT_colors[MEMORY_CACHE], "/", "cache");
   addbartext(CRT_colors[BAR_SHADOW], "                     ", "used");
   addbartext(CRT_colors[BAR_SHADOW], "/", "total");
   addattrstr(CRT_colors[BAR_BORDER], "]");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(line++, 0, "Swap bar:      ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addbartext(CRT_colors[SWAP], "", "used");
#ifdef HTOP_LINUX
   addbartext(CRT_colors[SWAP_CACHE], "/", "cache");
#else
   addbartext(CRT_colors[SWAP_CACHE], "      ", "");
#endif
   addbartext(CRT_colors[BAR_SHADOW], "                                    ", "used");
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
      mvaddstr(line + item, 41, helpRight[item].key);
      attrset((helpRight[item].roInactive && readonly) ? CRT_colors[HELP_SHADOW] : CRT_colors[DEFAULT_COLOR]);
      mvaddstr(line + item, 50, helpRight[item].info);
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
      Process* p = (Process*) Panel_get((Panel*)st->mainPanel, i);
      p->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   tagAllChildren((Panel*)st->mainPanel, p);
   return HTOP_OK;
}

static Htop_Reaction actionShowEnvScreen(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   EnvScreen* es = EnvScreen_new(p);
   InfoScreen_run((InfoScreen*)es);
   EnvScreen_delete((Object*)es);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionShowCommandScreen(State* st) {
   Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   CommandScreen* cmdScr = CommandScreen_new(p);
   InfoScreen_run((InfoScreen*)cmdScr);
   CommandScreen_delete((Object*)cmdScr);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

void Action_setBindings(Htop_Action* keys) {
   keys[' '] = actionTag;
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
   keys['P'] = actionSortByCPU;
   keys['S'] = actionSetup;
   keys['T'] = actionSortByTime;
   keys['U'] = actionUntagAll;
   keys['Z'] = actionTogglePauseProcessUpdate;
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
