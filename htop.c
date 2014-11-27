/*
htop - htop.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include "CRT.h"
#include "Panel.h"
#include "UsersTable.h"
#include "RichString.h"
#include "Settings.h"
#include "ScreenManager.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "String.h"
#include "ColumnsPanel.h"
#include "CategoriesPanel.h"
#include "SignalsPanel.h"
#include "TraceScreen.h"
#include "OpenFilesScreen.h"
#include "AffinityPanel.h"
#include "Platform.h"
#include "IncSet.h"
#include "Action.h"
#include "htop.h"

#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <locale.h>
#include <getopt.h>
#include <pwd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

//#link m

#define COPYRIGHT "(C) 2004-2014 Hisham Muhammad"

static void printVersionFlag() {
   fputs("htop " VERSION " - " COPYRIGHT "\n"
         "Released under the GNU GPL.\n\n",
         stdout);
   exit(0);
}

static const char* defaultFunctions[]  = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  ", NULL};
 
static void printHelpFlag() {
   fputs("htop " VERSION " - " COPYRIGHT "\n"
         "Released under the GNU GPL.\n\n"
         "-C --no-color               Use a monochrome color scheme\n"
         "-d --delay=DELAY            Set the delay between updates, in tenths of seconds\n"
         "-h --help                   Print this help screen\n"
         "-s --sort-key=COLUMN        Sort by COLUMN (try --sort-key=help for a list)\n"
         "-u --user=USERNAME          Show only processes of a given user\n"
         "-p --pid=PID,[,PID,PID...]  Show only the given PIDs\n"
         "-v --version          Print version info\n"
         "\n"
         "Long options may be passed with a single dash.\n\n"
         "Press F1 inside htop for online help.\n"
         "See 'man htop' for more information.\n",
         stdout);
   exit(0);
}

static struct { const char* key; const char* info; } helpLeft[] = {
   { .key = " Arrows: ", .info = "scroll process list" },
   { .key = " Digits: ", .info = "incremental PID search" },
   { .key = "   F3 /: ", .info = "incremental name search" },
   { .key = "   F4 \\: ",.info = "incremental name filtering" },
   { .key = "   F5 t: ", .info = "tree view" },
   { .key = "      u: ", .info = "show processes of a single user" },
   { .key = "      H: ", .info = "hide/show user threads" },
   { .key = "      K: ", .info = "hide/show kernel threads" },
   { .key = "      F: ", .info = "cursor follows process" },
   { .key = " F6 + -: ", .info = "expand/collapse tree" },
   { .key = "  P M T: ", .info = "sort by CPU%, MEM% or TIME" },
   { .key = "      I: ", .info = "invert sort order" },
   { .key = "   F6 >: ", .info = "select sort column" },
   { .key = NULL, .info = NULL }
};

static struct { const char* key; const char* info; } helpRight[] = {
   { .key = "  Space: ", .info = "tag process" },
   { .key = "      c: ", .info = "tag process and its children" },
   { .key = "      U: ", .info = "untag all processes" },
   { .key = "   F9 k: ", .info = "kill process/tagged processes" },
   { .key = "   F7 ]: ", .info = "higher priority (root only)" },
   { .key = "   F8 [: ", .info = "lower priority (+ nice)" },
#if (HAVE_LIBHWLOC || HAVE_NATIVE_AFFINITY)
   { .key = "      a: ", .info = "set CPU affinity" },
#endif
   { .key = "      i: ", .info = "set IO prority" },
   { .key = "      l: ", .info = "list open files with lsof" },
   { .key = "      s: ", .info = "trace syscalls with strace" },
   { .key = "         ", .info = "" },
   { .key = "   F2 S: ", .info = "setup" },
   { .key = "   F1 h: ", .info = "show this help screen" },
   { .key = "  F10 q: ", .info = "quit" },
   { .key = NULL, .info = NULL }
};

static void showHelp(ProcessList* pl) {
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
   if (pl->detailedCPUTime) {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "low"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_KERNEL], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_IRQ], "irq"); addstr("/");
      addattrstr(CRT_colors[CPU_SOFTIRQ], "soft-irq"); addstr("/");
      addattrstr(CRT_colors[CPU_STEAL], "steal"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "guest"); addstr("/");
      addattrstr(CRT_colors[CPU_IOWAIT], "io-wait");
      addattrstr(CRT_colors[BAR_SHADOW], " used%");
   } else {
      addattrstr(CRT_colors[CPU_NICE_TEXT], "low-priority"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_KERNEL], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_STEAL], "virtualiz");
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
      mvaddstr(7, 0, "In monochrome, meters are displayed through different chars, in order: |#*@$%&");
   }
   mvaddstr( 8, 0, " Status: R: running; S: sleeping; T: traced/stopped; Z: zombie; D: disk sleep");
   for (int i = 0; helpLeft[i].info; i++) { mvaddstr(9+i, 9,  helpLeft[i].info); }
   for (int i = 0; helpRight[i].info; i++) { mvaddstr(9+i, 49, helpRight[i].info); }
   attrset(CRT_colors[HELP_BOLD]);
   for (int i = 0; helpLeft[i].key;  i++) { mvaddstr(9+i, 0,  helpLeft[i].key); }
   for (int i = 0; helpRight[i].key; i++) { mvaddstr(9+i, 40, helpRight[i].key); }

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(23,0, "Press any key to return.");
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();
}

static const char* CategoriesFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void Setup_run(Settings* settings, const Header* header) {
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, true);
   CategoriesPanel* panelCategories = CategoriesPanel_new(settings, scr);
   ScreenManager_add(scr, (Panel*) panelCategories, FunctionBar_new(CategoriesFunctions, NULL, NULL), 16);
   CategoriesPanel_makeMetersPage(panelCategories);
   Panel* panelFocus;
   int ch;
   ScreenManager_run(scr, &panelFocus, &ch);
   ScreenManager_delete(scr);
}

static bool changePriority(Panel* panel, int delta) {
   bool anyTagged;
   bool ok = Action_foreachProcess(panel, (Action_ForeachProcessFn) Process_changePriorityBy, delta, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static void addUserToVector(int key, void* userCast, void* panelCast) {
   char* user = (char*) userCast;
   Panel* panel = (Panel*) panelCast;
   Panel_add(panel, (Object*) ListItem_new(user, key));
}

static bool setUserOnly(const char* userName, bool* userOnly, uid_t* userId) {
   struct passwd* user = getpwnam(userName);
   if (user) {
      *userOnly = true;
      *userId = user->pw_uid;
      return true;
   }
   return false;
}

static const char* getMainPanelValue(Panel* panel, int i) {
   Process* p = (Process*) Panel_get(panel, i);
   if (p)
      return p->comm;
   return "";
}

static void tagAllChildren(Panel* panel, Process* parent) {
   parent->tag = true;
   pid_t ppid = parent->pid;
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      if (!p->tag && p->ppid == ppid) {
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

static inline Htop_Reaction setSortKey(ProcessList* pl, ProcessField sortKey) {
   pl->sortKey = sortKey;
   pl->direction = 1;
   pl->treeView = false;
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction sortBy(Panel* panel, ProcessList* pl, Header* header) {
   Htop_Reaction reaction = HTOP_OK;
   Panel* sortPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem));
   Panel_setHeader(sortPanel, "Sort by");
   const char* fuFunctions[] = {"Sort  ", "Cancel ", NULL};
   ProcessField* fields = pl->fields;
   for (int i = 0; fields[i]; i++) {
      char* name = String_trim(Process_fieldNames[fields[i]]);
      Panel_add(sortPanel, (Object*) ListItem_new(name, fields[i]));
      if (fields[i] == pl->sortKey)
         Panel_setSelected(sortPanel, i);
      free(name);
   }
   ListItem* field = (ListItem*) Action_pickFromVector(panel, sortPanel, 15, fuFunctions, header);
   if (field) {
      reaction |= setSortKey(pl, field->key);
   }
   Object_delete(sortPanel);
   return reaction | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static void millisleep(unsigned long millisec) {
   struct timespec req = {
      .tv_sec = 0,
      .tv_nsec = millisec * 1000000L
   };
   while(nanosleep(&req,&req)==-1) {
      continue;
   }
}

// ----------------------------------------

static Htop_Reaction actionResize(Panel* panel) {
   Panel_resize(panel, COLS, LINES-(panel->y)-1);
   return HTOP_REDRAW_BAR;
}

static Htop_Reaction actionSortByMemory(Panel* panel, ProcessList* pl) {
   (void) panel;
   return setSortKey(pl, PERCENT_MEM);
}

static Htop_Reaction actionSortByCPU(Panel* panel, ProcessList* pl) {
   (void) panel;
   return setSortKey(pl, PERCENT_CPU);
}

static Htop_Reaction actionSortByTime(Panel* panel, ProcessList* pl) {
   (void) panel;
   return setSortKey(pl, TIME);
}

static Htop_Reaction actionToggleKernelThreads(Panel* panel, ProcessList* pl) {
   (void) panel;
   pl->hideKernelThreads = !pl->hideKernelThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleUserlandThreads(Panel* panel, ProcessList* pl) {
   (void) panel;
   pl->hideUserlandThreads = !pl->hideUserlandThreads;
   pl->hideThreads = pl->hideUserlandThreads;
   return HTOP_RECALCULATE | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionToggleTreeView(Panel* panel, ProcessList* pl) {
   (void) panel;
   pl->treeView = !pl->treeView;
   if (pl->treeView) pl->direction = 1;
   ProcessList_expandTree(pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS | HTOP_KEEP_FOLLOWING | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionIncFilter(Panel* panel, ProcessList* pl, Header* header, State* state) {
   (void) panel, (void) pl, (void) header;
   IncSet_activate(state->inc, INC_FILTER);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionIncSearch(Panel* panel, ProcessList* pl, Header* header, State* state) {
   (void) panel, (void) pl, (void) header;
   IncSet_activate(state->inc, INC_SEARCH);
   return HTOP_REFRESH | HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionHigherPriority(Panel* panel) {
   bool changed = changePriority(panel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionLowerPriority(Panel* panel) {
   bool changed = changePriority(panel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction actionInvertSortOrder(Panel* panel, ProcessList* pl) {
   (void) panel;
   ProcessList_invertSortOrder(pl);
   return HTOP_REFRESH | HTOP_SAVE_SETTINGS;
}

static Htop_Reaction actionSetSortColumn(Panel* panel, ProcessList* pl, Header* header) {
   return sortBy(panel, pl, header);
}

static Htop_Reaction actionExpandOrCollapse(Panel* panel) {
   bool changed = expandCollapse(panel);
   return changed ? HTOP_RECALCULATE : HTOP_OK;
}

static Htop_Reaction actionExpandCollapseOrSortColumn(Panel* panel, ProcessList* pl, Header* header) {
   return pl->treeView ? actionExpandOrCollapse(panel) : actionSetSortColumn(panel, pl, header);
}

static Htop_Reaction actionQuit() {
   return HTOP_QUIT;
}

static Htop_Reaction actionSetAffinity(Panel* panel, ProcessList* pl, Header* header) {
   if (pl->cpuCount == 1)
      return HTOP_OK;
#if (HAVE_LIBHWLOC || HAVE_NATIVE_AFFINITY)
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   Affinity* affinity = Process_getAffinity(p);
   if (!affinity) return HTOP_OK;
   Panel* affinityPanel = AffinityPanel_new(pl, affinity);
   Affinity_delete(affinity);

   const char* fuFunctions[] = {"Set    ", "Cancel ", NULL};
   void* set = Action_pickFromVector(panel, affinityPanel, 15, fuFunctions, header);
   if (set) {
      Affinity* affinity = AffinityPanel_getAffinity(affinityPanel);
      bool ok = Action_foreachProcess(panel, (Action_ForeachProcessFn) Process_setAffinity, (size_t) affinity, NULL);
      if (!ok) beep();
      Affinity_delete(affinity);
   }
   Panel_delete((Object*)affinityPanel);
#else
   (void) panel;
   (void) header;
#endif
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionKill(Panel* panel, ProcessList* pl, Header* header) {
   (void) pl;
   Panel* signalsPanel = (Panel*) SignalsPanel_new();
   const char* fuFunctions[] = {"Send  ", "Cancel ", NULL};
   ListItem* sgn = (ListItem*) Action_pickFromVector(panel, signalsPanel, 15, fuFunctions, header);
   if (sgn) {
      if (sgn->key != 0) {
         Panel_setHeader(panel, "Sending...");
         Panel_draw(panel, true);
         refresh();
         Action_foreachProcess(panel, (Action_ForeachProcessFn) Process_sendSignal, (size_t) sgn->key, NULL);
         napms(500);
      }
   }
   Panel_delete((Object*)signalsPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFilterByUser(Panel* panel, ProcessList* pl, Header* header, State* state) {
   Panel* usersPanel = Panel_new(0, 0, 0, 0, true, Class(ListItem));
   Panel_setHeader(usersPanel, "Show processes of:");
   UsersTable_foreach(state->ut, addUserToVector, usersPanel);
   Vector_insertionSort(usersPanel->items);
   ListItem* allUsers = ListItem_new("All users", -1);
   Panel_insert(usersPanel, 0, (Object*) allUsers);
   const char* fuFunctions[] = {"Show    ", "Cancel ", NULL};
   ListItem* picked = (ListItem*) Action_pickFromVector(panel, usersPanel, 20, fuFunctions, header);
   if (picked) {
      if (picked == allUsers) {
         pl->userOnly = false;
      } else {
         setUserOnly(ListItem_getRef(picked), &(pl->userOnly), &(pl->userId));
      }
   }
   Panel_delete((Object*)usersPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionFollow() {
   return HTOP_KEEP_FOLLOWING;
}

static Htop_Reaction actionSetup(Panel* panel, ProcessList* pl, Header* header, State* state) {
   (void) pl;
   Setup_run(state->settings, header);
   // TODO: shouldn't need this, colors should be dynamic
   int headerHeight = Header_calculateHeight(header);
   Panel_move(panel, 0, headerHeight);
   Panel_resize(panel, COLS, LINES-headerHeight-1);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static Htop_Reaction actionLsof(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   OpenFilesScreen* ts = OpenFilesScreen_new(p);
   OpenFilesScreen_run(ts);
   OpenFilesScreen_delete(ts);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionStrace(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   TraceScreen* ts = TraceScreen_new(p);
   TraceScreen_run(ts);
   TraceScreen_delete(ts);
   clear();
   CRT_enableDelay();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionTag(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   Process_toggleTag(p);
   Panel_onKey(panel, KEY_DOWN);
   return HTOP_OK;
}

static Htop_Reaction actionRedraw() {
   clear();
   return HTOP_REFRESH | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionHelp(Panel* panel, ProcessList* pl) {
   (void) panel;
   showHelp(pl);
   return HTOP_RECALCULATE | HTOP_REDRAW_BAR;
}

static Htop_Reaction actionUntagAll(Panel* panel) {
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      p->tag = false;
   }
   return HTOP_REFRESH;
}

static Htop_Reaction actionTagAllChildren(Panel* panel) {
   Process* p = (Process*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   tagAllChildren(panel, p);
   return HTOP_OK;
}

static void setBindings(Htop_Action* keys) {
   keys[KEY_RESIZE] = actionResize;
   keys['M'] = actionSortByMemory;
   keys['T'] = actionSortByTime;
   keys['P'] = actionSortByCPU;
   keys['H'] = actionToggleUserlandThreads;
   keys['K'] = actionToggleKernelThreads;
   keys['t'] = actionToggleTreeView;
   keys[KEY_F(5)] = actionToggleTreeView;
   keys[KEY_F(4)] = actionIncFilter;
   keys['\\'] = actionIncFilter;
   keys[KEY_F(3)] = actionIncSearch;
   keys['/'] = actionIncSearch;

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
   keys['+'] = actionExpandOrCollapse;
   keys['='] = actionExpandOrCollapse;
   keys['-'] = actionExpandOrCollapse;
   keys['u'] = actionFilterByUser;
   keys['F'] = actionFollow;
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
}

// ----------------------------------------


static void updateTreeFunctions(FunctionBar* fuBar, bool mode) {
   if (mode) {
      FunctionBar_setLabel(fuBar, KEY_F(5), "Sorted");
      FunctionBar_setLabel(fuBar, KEY_F(6), "Collap");
   } else {
      FunctionBar_setLabel(fuBar, KEY_F(5), "Tree  ");
      FunctionBar_setLabel(fuBar, KEY_F(6), "SortBy");
   }
}

int main(int argc, char** argv) {

   int delay = -1;
   bool userOnly = false;
   uid_t userId = 0;
   int usecolors = 1;
   char *argCopy;
   char *pid;
   Hashtable *pidWhiteList = NULL;

   int opt, opti=0;
   static struct option long_opts[] =
   {
      {"help",     no_argument,         0, 'h'},
      {"version",  no_argument,         0, 'v'},
      {"delay",    required_argument,   0, 'd'},
      {"sort-key", required_argument,   0, 's'},
      {"user",     required_argument,   0, 'u'},
      {"no-color", no_argument,         0, 'C'},
      {"no-colour",no_argument,         0, 'C'},
      {"pid",      required_argument,   0, 'p'},
      {0,0,0,0}
   };
   int sortKey = 0;

   char *lc_ctype = getenv("LC_CTYPE");
   if(lc_ctype != NULL)
      setlocale(LC_CTYPE, lc_ctype);
   else if ((lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hvCs:d:u:p:", long_opts, &opti))) {
      if (opt == EOF) break;
      switch (opt) {
         case 'h':
            printHelpFlag();
            break;
         case 'v':
            printVersionFlag();
            break;
         case 's':
            if (strcmp(optarg, "help") == 0) {
               for (int j = 1; j < LAST_PROCESSFIELD; j++)
                  printf ("%s\n", Process_fieldNames[j]);
               exit(0);
            }

            sortKey = ColumnsPanel_fieldNameToIndex(optarg);
            if (sortKey == -1) {
               fprintf(stderr, "Error: invalid column \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'd':
            if (sscanf(optarg, "%16d", &delay) == 1) {
               if (delay < 1) delay = 1;
               if (delay > 100) delay = 100;
            } else {
               fprintf(stderr, "Error: invalid delay value \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'u':
            if (!setUserOnly(optarg, &userOnly, &userId)) {
               fprintf(stderr, "Error: invalid user \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'C':
            usecolors=0;
            break;
        case 'p': {
            argCopy = strdup(optarg);
            char* saveptr;
            pid = strtok_r(argCopy, ",", &saveptr);

            if( !pidWhiteList ) {
               pidWhiteList = Hashtable_new(8, false);
            }

            while( pid ) {
                unsigned int num_pid = atoi(pid);
                Hashtable_put(pidWhiteList, num_pid, (void *) 1);
                pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         default:
            exit(1);
      }
   }

#ifdef HAVE_PROC
   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }
#endif
   
#ifdef HAVE_LIBNCURSESW
   char *locale = setlocale(LC_ALL, NULL);
   if (locale == NULL || locale[0] == '\0')
      locale = setlocale(LC_CTYPE, NULL);
   if (locale != NULL &&
       (strstr(locale, "UTF-8") ||
        strstr(locale, "utf-8") ||
        strstr(locale, "UTF8")  ||
        strstr(locale, "utf8")))
      CRT_utf8 = true;
   else
      CRT_utf8 = false;
#endif

   UsersTable* ut = UsersTable_new();
   ProcessList* pl = ProcessList_new(ut, pidWhiteList);
   pl->userOnly = userOnly;
   pl->userId = userId;
   Process_setupColumnWidths();
   
   Header* header = Header_new(pl);
   Settings* settings = Settings_new(pl, header, pl->cpuCount);
   int headerHeight = Header_calculateHeight(header);

   // FIXME: move delay code to settings
   if (delay != -1)
      settings->delay = delay;
   if (!usecolors) 
      settings->colorScheme = COLORSCHEME_MONOCHROME;

   CRT_init(settings->delay, settings->colorScheme);

   Panel* panel = Panel_new(0, headerHeight, COLS, LINES - headerHeight - 2, false, &Process_class);
   ProcessList_setPanel(pl, panel);
   
   FunctionBar* defaultBar = FunctionBar_new(defaultFunctions, NULL, NULL);
   updateTreeFunctions(defaultBar, pl->treeView);
   
   if (sortKey > 0) {
      pl->sortKey = sortKey;
      pl->treeView = false;
      pl->direction = 1;
   }
   ProcessList_printHeader(pl, Panel_getHeader(panel));

   IncSet* inc = IncSet_new(defaultBar);

   ProcessList_scan(pl);
   millisleep(75);
   ProcessList_scan(pl);
   
   FunctionBar_draw(defaultBar, NULL);
   
   int acc = 0;
   bool follow = false;
 
   struct timeval tv;
   double oldTime = 0.0;

   int ch = ERR;
   int closeTimeout = 0;

   bool drawPanel = true;
   
   bool collapsed = false;
   
   Htop_Action keys[KEY_MAX] = { NULL };
   setBindings(keys);
   Platform_setBindings(keys);
   
   State state = {
      .inc = inc,
      .settings = settings,
      .ut = ut,
   };

   bool quit = false;
   int sortTimeout = 0;
   int resetSortTimeout = 5;
   bool doRefresh = true;
   bool forceRecalculate = false;
   
   while (!quit) {
      gettimeofday(&tv, NULL);
      double newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);
      bool timeToRecalculate = (newTime - oldTime > settings->delay);
      if (newTime < oldTime) timeToRecalculate = true; // clock was adjusted?
      int following = follow ? Action_selectedPid(panel) : -1;
      if (timeToRecalculate) {
         Header_draw(header);
         oldTime = newTime;
      }
      if (doRefresh) {
         if (timeToRecalculate || forceRecalculate) {
            ProcessList_scan(pl);
            forceRecalculate = false;
         }
         if (sortTimeout == 0 || pl->treeView) {
            ProcessList_sort(pl);
            sortTimeout = 1;
         }
         ProcessList_rebuildPanel(pl, true, following, IncSet_filter(inc));
         drawPanel = true;
      }
      doRefresh = true;
      
      if (pl->treeView) {
         Process* p = (Process*) Panel_getSelected(panel);
         if (p) {
            if (!p->showChildren && !collapsed) {
               FunctionBar_setLabel(defaultBar, KEY_F(6), "Expand");
               FunctionBar_draw(defaultBar, NULL);
            } else if (p->showChildren && collapsed) {
               FunctionBar_setLabel(defaultBar, KEY_F(6), "Collap");
               FunctionBar_draw(defaultBar, NULL);
            }
            collapsed = !p->showChildren;
         }
      } else {
         collapsed = false;
      }

      if (drawPanel) {
         Panel_draw(panel, true);
      }
      
      int prev = ch;
      if (inc->active)
         move(LINES-1, CRT_cursorX);
      ch = getch();

      if (ch == ERR) {
         if (!inc->active)
            sortTimeout--;
         if (prev == ch && !timeToRecalculate) {
            closeTimeout++;
            if (closeTimeout == 100) {
               break;
            }
         } else
            closeTimeout = 0;
         drawPanel = false;
         continue;
      }
      drawPanel = true;

      Htop_Reaction reaction = HTOP_OK;

      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.bstate & BUTTON1_CLICKED) {
               if (mevent.y == panel->y) {
                  int x = panel->scrollH + mevent.x + 1;
                  ProcessField field = ProcessList_keyAt(pl, x);
                  if (field == pl->sortKey) {
                     ProcessList_invertSortOrder(pl);
                     pl->treeView = false;
                     reaction |= HTOP_REDRAW_BAR;
                  } else {
                     reaction |= setSortKey(pl, field);
                  }
                  sortTimeout = 0;
                  ch = ERR;
               } else if (mevent.y >= panel->y + 1 && mevent.y < LINES - 1) {
                  Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                  follow = true;
                  ch = ERR;
               } if (mevent.y == LINES - 1) {
                  ch = FunctionBar_synthesizeEvent(inc->bar, mevent.x);
               }
            } else if (mevent.bstate & BUTTON4_CLICKED) {
               ch = KEY_UP;
            #if NCURSES_MOUSE_VERSION > 1
            } else if (mevent.bstate & BUTTON5_CLICKED) {
               ch = KEY_DOWN;
            #endif
            }
         }
      }

      if (inc->active) {
         doRefresh = IncSet_handleKey(inc, ch, panel, getMainPanelValue, NULL);
         if (!inc->active) {
            follow = true;
         }
         continue;
      }
      
      if (isdigit((char)ch)) {
         if (Panel_size(panel) == 0) continue;
         pid_t pid = ch-48 + acc;
         for (int i = 0; i < ProcessList_size(pl); i++) {
            Panel_setSelected(panel, i);
            Process* p = (Process*) Panel_getSelected(panel);
            if (p && p->pid == pid) {
               break;
            }
         }
         acc = pid * 10;
         if (acc > 10000000)
            acc = 0;
         continue;
      } else {
         acc = 0;
      }

      if(ch != ERR && keys[ch]) {
         reaction |= (keys[ch])(panel, pl, header, &state);
      } else {
         doRefresh = false;
         sortTimeout = resetSortTimeout;
         Panel_onKey(panel, ch);
      }
      
      // Reaction handlers:

      if (reaction & HTOP_REDRAW_BAR) {
         updateTreeFunctions(defaultBar, pl->treeView);
         IncSet_drawBar(inc);
      }
      if (reaction & HTOP_UPDATE_PANELHDR) {
         ProcessList_printHeader(pl, Panel_getHeader(panel));
      }
      if (reaction & HTOP_REFRESH) {
         doRefresh = true;
         sortTimeout = 0;
      }      
      if (reaction & HTOP_RECALCULATE) {
         forceRecalculate = true;
         sortTimeout = 0;
      }
      if (reaction & HTOP_SAVE_SETTINGS) {
         settings->changed = true;
      }
      if (reaction & HTOP_QUIT) {
         quit = true;
      }
      follow = (reaction & HTOP_KEEP_FOLLOWING);
   }
   attron(CRT_colors[RESET_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[RESET_COLOR]);
   refresh();
   
   CRT_done();
   if (settings->changed)
      Settings_write(settings);
   Header_delete(header);
   ProcessList_delete(pl);
   
   FunctionBar_delete((Object*)defaultBar);
   Panel_delete((Object*)panel);
   
   IncSet_delete(inc);
   UsersTable_delete(ut);
   Settings_delete(settings);
   
   if(pidWhiteList) {
      Hashtable_delete(pidWhiteList);
   }
   return 0;
}
