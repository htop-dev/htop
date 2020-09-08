/*
htop - Action.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "Action.h"
#include "Affinity.h"
#include "AffinityPanel.h"
#include "CategoriesPanel.h"
#include "CRT.h"
#include "EnvScreen.h"
#include "MainPanel.h"
#include "OpenFilesScreen.h"
#include "Process.h"
#include "ScreenManager.h"
#include "SignalsPanel.h"
#include "StringUtils.h"
#include "TraceScreen.h"
#include "Platform.h"

#include <ctype.h>
#include <math.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/time.h>

Object* Action_pickFromVector(State* st, Panel* list, int x, bool followProcess) {
   Panel* panel = st->panel;
   Header* header = st->header;
   Settings* settings = st->settings;

   int y = panel->y;
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, settings, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, x - 1);
   ScreenManager_add(scr, panel, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   int pid = followProcess ? MainPanel_selectedPid((MainPanel*)panel) : -1;
   if (followProcess && header->pl->following == -1) {
      header->pl->following = pid;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch);
   if (unfollow) {
      header->pl->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move(panel, 0, y);
   Panel_resize(panel, COLS, LINES-y-1);
   if (panelFocus == list && ch == 13) {
      if (followProcess) {
         Process* selected = (Process*)Panel_getSelected(panel);
         if (selected && selected->pid == pid)
            return Panel_getSelected(list);
         else
            beep();
      } else {
         return Panel_getSelected(list);
      }

   }
   return NULL;
}

// ----------------------------------------

static void Action_runSetup(Settings* settings, const Header* header, ProcessList* pl) {
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, settings, true);
   CategoriesPanel* panelCategories = CategoriesPanel_new(scr, settings, (Header*) header, pl);
   ScreenManager_add(scr, (Panel*) panelCategories, 16);
   CategoriesPanel_makeMetersPage(panelCategories);
   Panel* panelFocus;
   int ch;
   ScreenManager_run(scr, &panelFocus, &ch);
   ScreenManager_delete(scr);
   if (settings->changed) {
      Header_writeBackToSettings(header);
   }
}

static bool changePriority(MainPanel* panel, int delta) {
   bool anyTagged;
   bool ok = MainPanel_foreachProcess(panel, (MainPanel_ForeachProcessFn) Process_changePriorityBy, (Arg){ .i = delta }, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static void addUserToVector(int key, void* userCast, void* panelCast) {
   char* user = (char*) userCast;
   Panel* panel = (Panel*) panelCast;
   Panel_add(panel, (Object*) ListItem_new(user, key));
}

bool Action_setUserOnly(const char* userName, uid_t* userId) {
   struct passwd* user = getpwnam(userName);
   if (user) {
      *userId = user->pw_uid;
      return true;
   }
   *userId = -1;
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
   if (!p) return false;
   p->showChildren = !p->showChildren;
   return true;
}

static bool collapseIntoParent(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return false;
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
   settings->sortKey = sortKey;
   settings->direction = 1;
   settings->treeView = false;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction sortBy(State* st) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem), FunctionBar_newEnterEsc("Sort   ", "Cancel "));
   Panel_setHeader(sortPanel, "Sort by");
   ProcessField* fields = st->settings->fields;
   for (int i = 0; fields[i]; i++) {
      char* name = String_trim(Process_fields[fields[i]].name);
      Panel_add(sortPanel, (Object*) ListItem_new(name, fields[i]));
      if (fields[i] == st->settings->sortKey)
         Panel_setSelected(sortPanel, i);
      free(name);
   }
   ListItem* field = (ListItem*) Action_pickFromVector(st, sortPanel, 15, false);
   if (field) {
      reaction |= Action_setSortKey(st->settings, field->key);
   }
   Object_delete(sortPanel);
   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

// ----------------------------------------

static Htop_Reaction actionResize(State* st) {
   clear();
   Panel_resize(st->panel, COLS, LINES-(st->panel->y)-1);
   return HTOP_REDRAW_BAR;
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
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleUserlandThreads(State* st) {
   st->settings->hideUserlandThreads = !st->settings->hideUserlandThreads;
   st->settings->hideThreads = st->settings->hideUserlandThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleProgramPath(State* st) {
   st->settings->showProgramPath = !st->settings->showProgramPath;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleTreeView(State* st) {
   st->settings->treeView = !st->settings->treeView;
   if (st->settings->treeView) st->settings->direction = 1;
   ProcessList_expandTree(st->pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionIncFilter(State* st) {
   IncSet* inc = ((MainPanel*)st->panel)->inc;
   IncSet_activate(inc, INC_FILTER, st->panel);
   st->pl->incFilter = IncSet_filter(inc);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(State* st) {
   IncSet_reset(((MainPanel*)st->panel)->inc, INC_SEARCH);
   IncSet_activate(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncNext(State* st) {
   IncSet_next(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel, (IncMode_GetPanelValue) MainPanel_getValue);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncPrev(State* st) {
   IncSet_prev(((MainPanel*)st->panel)->inc, INC_SEARCH, st->panel, (IncMode_GetPanelValue) MainPanel_getValue);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionHigherPriority(State* st) {
   bool changed = changePriority((MainPanel*)st->panel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(State* st) {
   bool changed = changePriority((MainPanel*)st->panel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(State* st) {
   Settings_invertSortOrder(st->settings);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionSetSortColumn(State* st) {
   return sortBy(st);
}

static Htop_Reaction actionExpandOrCollapse(State* st) {
   bool changed = expandCollapse(st->panel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionCollapseIntoParent(State* st) {
   if (!st->settings->treeView) {
      return HTOP_OK;
   }
   bool changed = collapseIntoParent(st->panel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(State* st) {
   return st->settings->treeView ? actionExpandOrCollapse(st) : actionSetSortColumn(st);
}

static Htop_Reaction actionQuit() {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(State* st) {
   if (st->pl->cpuCount == 1)
      return HTOP_OK;
#if (HAVE_LIBHWLOC || HAVE_LINUX_AFFINITY)
   Panel* panel = st->panel;

   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   Affinity* affinity1 = Affinity_get(p, st->pl);
   if (!affinity1) return HTOP_OK;
   int width;
   Panel* affinityPanel = AffinityPanel_new(st->pl, affinity1, &width);
   width += 1; /* we add a gap between the panels */
   Affinity_delete(affinity1);

   void* set = Action_pickFromVector(st, affinityPanel, width, true);
   if (set) {
      Affinity* affinity2 = AffinityPanel_getAffinity(affinityPanel, st->pl);
      bool ok = MainPanel_foreachProcess((MainPanel*)panel, (MainPanel_ForeachProcessFn) Affinity_set, (Arg){ .v = affinity2 }, NULL);
      if (!ok) beep();
      Affinity_delete(affinity2);
   }
   Panel_delete((Object*)affinityPanel);
#endif
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionKill(State* st) {
   Panel* signalsPanel = (Panel*) SignalsPanel_new();
   ListItem* sgn = (ListItem*) Action_pickFromVector(st, signalsPanel, 15, true);
   if (sgn) {
      if (sgn->key != 0) {
         Panel_setHeader(st->panel, "Sending...");
         Panel_draw(st->panel, true);
         refresh();
         MainPanel_foreachProcess((MainPanel*)st->panel, (MainPanel_ForeachProcessFn) Process_sendSignal, (Arg){ .i = sgn->key }, NULL);
         napms(500);
      }
   }
   Panel_delete((Object*)signalsPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(State* st) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem), FunctionBar_newEnterEsc("Show   ", "Cancel "));
   Panel_setHeader(usersPanel, "Show processes of:");
   UsersTable_foreach(st->ut, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("All users", -1);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   ListItem* picked = (ListItem*) Action_pickFromVector(st, usersPanel, 20, false);
   if (picked) {
      if (picked == allUsers) {
         st->pl->userId = -1;
      } else {
         Action_setUserOnly(ListItem_getRef(picked), &(st->pl->userId));
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

Htop_Reaction Action_follow(State* st) {
   st->pl->following = MainPanel_selectedPid((MainPanel*)st->panel);
   Panel_setSelectionColor(st->panel, CRT_colors[PANEL_SELECTION_FOLLOW]);
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(State* st) {
   Action_runSetup(st->settings, st->header, st->pl);
   // TODO: shouldn't need this, colors should be dynamic
   int headerHeight = Header_calculateHeight(st->header);
   Panel_move(st->panel, 0, headerHeight);
   Panel_resize(st->panel, COLS, LINES-headerHeight-1);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionLsof(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   OpenFilesScreen* ofs = OpenFilesScreen_new(p);
   InfoScreen_run((InfoScreen*)ofs);
   OpenFilesScreen_delete((Object*)ofs);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
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
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   Process_toggleTag(p);
   Panel_onKey(st->panel, KEY_DOWN);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw() {
   clear();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static const struct { const char* key; const char* info; } helpLeft[] = {
   { .key = " Arrows: ", .info = "scroll process list" },
   { .key = " Digits: ", .info = "incremental PID search" },
   { .key = "   F3 /: ", .info = "incremental name search" },
   { .key = "   F4 \\: ",.info = "incremental name filtering" },
   { .key = "   F5 t: ", .info = "tree view" },
   { .key = "      p: ", .info = "toggle program path" },
   { .key = "      u: ", .info = "show processes of a single user" },
   { .key = "      H: ", .info = "hide/show user process threads" },
   { .key = "      K: ", .info = "hide/show kernel threads" },
   { .key = "      F: ", .info = "cursor follows process" },
   { .key = " F6 + -: ", .info = "expand/collapse tree" },
   { .key = "  P M T: ", .info = "sort by CPU%, MEM% or TIME" },
   { .key = "      I: ", .info = "invert sort order" },
   { .key = " F6 > .: ", .info = "select sort column" },
   { .key = NULL, .info = NULL }
};

static const struct { const char* key; const char* info; } helpRight[] = {
   { .key = "  Space: ", .info = "tag process" },
   { .key = "      c: ", .info = "tag process and its children" },
   { .key = "      U: ", .info = "untag all processes" },
   { .key = "   F9 k: ", .info = "kill process/tagged processes" },
   { .key = "   F7 ]: ", .info = "higher priority (root only)" },
   { .key = "   F8 [: ", .info = "lower priority (+ nice)" },
#if (HAVE_LIBHWLOC || HAVE_LINUX_AFFINITY)
   { .key = "      a: ", .info = "set CPU affinity" },
#endif
   { .key = "      e: ", .info = "show process environment" },
   { .key = "      i: ", .info = "set IO priority" },
   { .key = "      l: ", .info = "list open files with lsof" },
   { .key = "      s: ", .info = "trace syscalls with strace" },
   { .key = "         ", .info = "" },
   { .key = " F2 C S: ", .info = "setup" },
   { .key = "   F1 h: ", .info = "show this help screen" },
   { .key = "  F10 q: ", .info = "quit" },
   { .key = NULL, .info = NULL }
};

static Htop_Reaction actionHelp(State* st) {
   Settings* settings = st->settings;

   clear();
   attrset(CRT_colors[HELP_BOLD]);

   for (int i = 0; i < LINES-1; i++)
      mvhline(i, 0, ' ', COLS);

   mvaddstr(0, 0, "htop " VERSION " - " COPYRIGHT);
   mvaddstr(1, 0, "Released under the GNU GPL. See 'man' page for more info.");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(3, 0, "CPU usage bar: ");
   #define addattrstr(a,s) attrset(a);addstr(s)
   addattrstr(CRT_colors[BAR_BORDER], "[");
   if (settings->detailedCPUTime) {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "low"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_SYSTEM], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_IRQ], "irq"); addstr("/");
      addattrstr(CRT_colors[CPU_SOFTIRQ], "soft-irq"); addstr("/");
      addattrstr(CRT_colors[CPU_STEAL], "steal"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "guest"); addstr("/");
      addattrstr(CRT_colors[CPU_IOWAIT], "io-wait");
      addattrstr(CRT_colors[BAR_SHADOW], " used%");
   } else {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "low-priority"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_SYSTEM], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "virtualiz");
      addattrstr(CRT_colors[BAR_SHADOW], "               used%");
   }
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(4, 0, "Memory bar:    ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[MEMORY_USED], "used"); addstr("/");
   addattrstr(CRT_colors[MEMORY_BUFFERS_TEXT], "buffers"); addstr("/");
   addattrstr(CRT_colors[MEMORY_CACHE], "cache");
   addattrstr(CRT_colors[BAR_SHADOW], "                            used/total");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(5, 0, "Swap bar:      ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[SWAP], "used");
   addattrstr(CRT_colors[BAR_SHADOW], "                                          used/total");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(6,0, "Type and layout of header meters are configurable in the setup screen.");
   if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
      mvaddstr(7, 0, "In monochrome, meters display as different chars, in order: |#*@$%&.");
   }
   mvaddstr( 8, 0, " Status: R: running; S: sleeping; T: traced/stopped; Z: zombie; D: disk sleep");
   for (int i = 0; helpLeft[i].info; i++) { mvaddstr(9+i, 9,  helpLeft[i].info); }
   for (int i = 0; helpRight[i].info; i++) { mvaddstr(9+i, 49, helpRight[i].info); }
   attrset(CRT_colors[HELP_BOLD]);
   for (int i = 0; helpLeft[i].key;  i++) { mvaddstr(9+i, 0,  helpLeft[i].key); }
   for (int i = 0; helpRight[i].key; i++) { mvaddstr(9+i, 40, helpRight[i].key); }
   attrset(CRT_colors[PROCESS_THREAD]);
   mvaddstr(16, 32, "threads");
   mvaddstr(17, 26, "threads");
   attrset(CRT_colors[DEFAULT_COLOR]);

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(23,0, "Press any key to return.");
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();

   return HTOP_RECALCULATE | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionUntagAll(State* st) {
   for (int i = 0; i < Panel_size(st->panel); i++) {
      Process* p = (Process*) Panel_get(st->panel, i);
      p->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   tagAllChildren(st->panel, p);
   return HTOP_OK;
}

static Htop_Reaction actionShowEnvScreen(State* st) {
   Process* p = (Process*) Panel_getSelected(st->panel);
   if (!p) return HTOP_OK;
   EnvScreen* es = EnvScreen_new(p);
   InfoScreen_run((InfoScreen*)es);
   EnvScreen_delete((Object*)es);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}


void Action_setBindings(Htop_Action* keys) {
   keys[KEY_RESIZE] = actionResize;
   keys['M'] = actionSortByMemory;
   keys['T'] = actionSortByTime;
   keys['P'] = actionSortByCPU;
   keys['H'] = actionToggleUserlandThreads;
   keys['K'] = actionToggleKernelThreads;
   keys['p'] = actionToggleProgramPath;
   keys['t'] = actionToggleTreeView;
   keys[KEY_F(5)] = actionToggleTreeView;
   keys[KEY_F(4)] = actionIncFilter;
   keys['\\'] = actionIncFilter;
   keys[KEY_F(3)] = actionIncSearch;
   keys['/'] = actionIncSearch;
   keys['n'] = actionIncNext;
   keys['N'] = actionIncPrev;

   keys[']'] = actionHigherPriority;
   keys[KEY_F(7)] = actionHigherPriority;
   keys['['] = actionLowerPriority;
   keys[KEY_F(8)] = actionLowerPriority;
   keys['I'] = actionInvertSortOrder;
   keys[KEY_F(6)] = actionExpandCollapseOrSortColumn;
   keys[KEY_F(18)] = actionExpandCollapseOrSortColumn;
   keys['<'] = actionSetSortColumn;
   keys[','] = actionSetSortColumn;
   keys['>'] = actionSetSortColumn;
   keys['.'] = actionSetSortColumn;
   keys[KEY_F(10)] = actionQuit;
   keys['q'] = actionQuit;
   keys['a'] = actionSetAffinity;
   keys[KEY_F(9)] = actionKill;
   keys['k'] = actionKill;
   keys[KEY_RECLICK] = actionExpandOrCollapse;
   keys['+'] = actionExpandOrCollapse;
   keys['='] = actionExpandOrCollapse;
   keys['-'] = actionExpandOrCollapse;
   keys['\177'] = actionCollapseIntoParent;
   keys['u'] = actionFilterByUser;
   keys['F'] = Action_follow;
   keys['S'] = actionSetup;
   keys['C'] = actionSetup;
   keys[KEY_F(2)] = actionSetup;
   keys['l'] = actionLsof;
   keys['s'] = actionStrace;
   keys[' '] = actionTag;
   keys['\014'] = actionRedraw; // Ctrl+L
   keys[KEY_F(1)] = actionHelp;
   keys['h'] = actionHelp;
   keys['?'] = actionHelp;
   keys['U'] = actionUntagAll;
   keys['c'] = actionTagAllChildren;
   keys['e'] = actionShowEnvScreen;
}
