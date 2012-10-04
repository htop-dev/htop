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
#include "IOPriorityPanel.h"

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

//#link m

#define INCSEARCH_MAX 40

#define COPYRIGHT "(C) 2004-2011 Hisham Muhammad"

static void printVersionFlag() {
   fputs("htop " VERSION " - " COPYRIGHT "\n"
         "Released under the GNU GPL.\n\n",
         stdout);
   exit(0);
}

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
      addattrstr(CRT_colors[CPU_NICE], "low"); addstr("/");
      addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
      addattrstr(CRT_colors[CPU_KERNEL], "kernel"); addstr("/");
      addattrstr(CRT_colors[CPU_IRQ], "irq"); addstr("/");
      addattrstr(CRT_colors[CPU_SOFTIRQ], "soft-irq"); addstr("/");
      addattrstr(CRT_colors[CPU_IOWAIT], "io-wait"); addstr("/");
      addattrstr(CRT_colors[CPU_STEAL], "steal"); addstr("/");
      addattrstr(CRT_colors[CPU_GUEST], "guest");
      addattrstr(CRT_colors[BAR_SHADOW], " used%");
   } else {
      addattrstr(CRT_colors[CPU_NICE], "low-priority"); addstr("/");
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
   addattrstr(CRT_colors[MEMORY_BUFFERS], "buffers"); addstr("/");
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
   mvaddstr( 9, 0, " Arrows: scroll process list             F5 t: tree view");
   mvaddstr(10, 0, " Digits: incremental PID search             u: show processes of a single user");
   mvaddstr(11, 0, "   F3 /: incremental name search            H: hide/show user threads");
   mvaddstr(12, 0, "   F4 \\: incremental name filtering         K: hide/show kernel threads");
   mvaddstr(13, 0, "  Space: tag processes                      F: cursor follows process");
   mvaddstr(14, 0, "      U: untag all processes              + -: expand/collapse tree");
   mvaddstr(15, 0, "   F9 k: kill process/tagged processes  P M T: sort by CPU%, MEM% or TIME");
   mvaddstr(16, 0, "   ] F7: higher priority (root only)        i: set IO priority");
   mvaddstr(17, 0, "   [ F8: lower priority (+ nice)            I: invert sort order");
#if (HAVE_LIBHWLOC || HAVE_NATIVE_AFFINITY)
   if (pl->cpuCount > 1)
      mvaddstr(18, 0, "      a: set CPU affinity                F6 >: select sort column");
   else
#endif
      mvaddstr(18, 0, "                                         F6 >: select sort column");
   mvaddstr(19, 0, "   F2 S: setup                              l: list open files with lsof");
   mvaddstr(20, 0, "   F1 h: show this help screen              s: trace syscalls with strace");
   mvaddstr(21, 0, "  F10 q: quit");

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr( 9, 0, " Arrows"); mvaddstr( 9,40, " F5 t");
   mvaddstr(10, 0, " Digits"); mvaddstr(10,40, "    u");
   mvaddstr(11, 0, "   F3 /"); mvaddstr(11,40, "    H");
   mvaddstr(12, 0, "   F4 \\"); mvaddstr(12,40, "    K");
   mvaddstr(13, 0, "  Space"); mvaddstr(13,40, "    F");
   mvaddstr(14, 0, "      U"); mvaddstr(14,40, "  + -");
   mvaddstr(15, 0, "   F9 k"); mvaddstr(15,40, "P M T");
   mvaddstr(16, 0, "   ] F7"); mvaddstr(16,40, "    i");
   mvaddstr(17, 0, "   [ F8"); mvaddstr(17,40, "    I");
                               mvaddstr(18,40, " F6 >");
#if (HAVE_LIBHWLOC || HAVE_NATIVE_AFFINITY)
   if (pl->cpuCount > 1)
      mvaddstr(18, 0, "      a:");
#endif
   mvaddstr(19, 0, "   F2 S"); mvaddstr(19,40, "    l");
   mvaddstr(20, 0, " ? F1 h"); mvaddstr(20,40, "    s");
   mvaddstr(21, 0, "  F10 q");
   attrset(CRT_colors[DEFAULT_COLOR]);

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

typedef bool(*ForeachProcessFn)(Process*, size_t);

static bool foreachProcess(Panel* panel, ForeachProcessFn fn, int arg, bool* wasAnyTagged) {
   bool ok = true;
   bool anyTagged = false;
   for (int i = 0; i < Panel_size(panel); i++) {
      Process* p = (Process*) Panel_get(panel, i);
      if (p->tag) {
         ok = fn(p, arg) && ok;
         anyTagged = true;
      }
   }
   if (!anyTagged) {
      Process* p = (Process*) Panel_getSelected(panel);
      if (p) ok = fn(p, arg) && ok;
   }
   if (wasAnyTagged)
      *wasAnyTagged = anyTagged;
   return ok;
}

static bool changePriority(Panel* panel, int delta) {
   bool anyTagged;
   bool ok = foreachProcess(panel, (ForeachProcessFn) Process_changePriorityBy, delta, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static Object* pickFromVector(Panel* panel, Panel* list, int x, int y, const char** keyLabels, FunctionBar* prevBar, Header* header) {
   const char* fuKeys[] = {"Enter", "Esc", NULL};
   int fuEvents[] = {13, 27};
   if (!list->eventHandler)
      Panel_setEventHandler(list, Panel_selectByTyping);
   ScreenManager* scr = ScreenManager_new(0, y, 0, -1, HORIZONTAL, header, false);
   scr->allowFocusChange = false;
   ScreenManager_add(scr, list, FunctionBar_new(keyLabels, fuKeys, fuEvents), x - 1);
   ScreenManager_add(scr, panel, NULL, -1);
   Panel* panelFocus;
   int ch;
   bool unfollow = false;
   if (header->pl->following == -1) {
      Process* p = (Process*)Panel_getSelected(panel);
      header->pl->following = p ? p->pid : -1;
      unfollow = true;
   }
   ScreenManager_run(scr, &panelFocus, &ch);
   if (unfollow) {
      header->pl->following = -1;
   }
   ScreenManager_delete(scr);
   Panel_move(panel, 0, y);
   Panel_resize(panel, COLS, LINES-y-1);
   FunctionBar_draw(prevBar, NULL);
   if (panelFocus == list && ch == 13) {
      return Panel_getSelected(list);
   }
   return NULL;
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

static inline void setSortKey(ProcessList* pl, ProcessField sortKey, Panel* panel, Settings* settings) {
   pl->sortKey = sortKey;
   pl->direction = 1;
   pl->treeView = false;
   settings->changed = true;
   ProcessList_printHeader(pl, Panel_getHeader(panel));
}

typedef struct IncBuffer_ {
   char buffer[INCSEARCH_MAX];
   int index;
   FunctionBar* bar;
} IncBuffer;

static void IncBuffer_reset(IncBuffer* inc) {
   inc->index = 0;
   inc->buffer[0] = 0;
}

int main(int argc, char** argv) {

   int delay = -1;
   bool userOnly = false;
   uid_t userId = 0;
   int usecolors = 1;
   TreeType treeType = TREE_TYPE_AUTO;
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
            if (sscanf(optarg, "%d", &delay) == 1) {
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
        case 'p':
            argCopy = strdup(optarg);
            pid = strtok(argCopy, ",");

            if( !pidWhiteList ) {
               pidWhiteList = Hashtable_new(8, false);
            }

            while( pid ) {
                unsigned int num_pid = atoi(pid);
                Hashtable_put(pidWhiteList, num_pid, (void *) 1);
                pid = strtok(NULL, ",");
            }
            free(argCopy);

            break;
         default:
            exit(1);
      }
   }


   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }

   int quit = 0;
   int refreshTimeout = 0;
   int resetRefreshTimeout = 5;
   bool doRefresh = true;
   bool doRecalculate = false;
   Settings* settings;
   
   Panel* killPanel = NULL;
   
   ProcessList* pl = NULL;
   UsersTable* ut = UsersTable_new();

   pl = ProcessList_new(ut, pidWhiteList);
   Process_getMaxPid();
   
   Header* header = Header_new(pl);
   settings = Settings_new(pl, header, pl->cpuCount);
   int headerHeight = Header_calculateHeight(header);

   // FIXME: move delay code to settings
   if (delay != -1)
      settings->delay = delay;
   if (!usecolors) 
      settings->colorScheme = COLORSCHEME_MONOCHROME;

   if (treeType == TREE_TYPE_AUTO) {
#ifdef HAVE_LIBNCURSESW
      char *locale = setlocale(LC_ALL, NULL);
      if (locale == NULL || locale[0] == '\0')
         locale = setlocale(LC_CTYPE, NULL);
      if (locale != NULL &&
          (strstr(locale, "UTF-8") ||
           strstr(locale, "utf-8") ||
           strstr(locale, "UTF8")  ||
           strstr(locale, "utf8")))
         treeType = TREE_TYPE_UTF8;
      else
         treeType = TREE_TYPE_ASCII;
#else
      treeType = TREE_TYPE_ASCII;
#endif
   }
   switch (treeType) {
   default:
   case TREE_TYPE_ASCII:
      pl->treeStr = ProcessList_treeStrAscii;
      break;
   case TREE_TYPE_UTF8:
      pl->treeStr = ProcessList_treeStrUtf8;
      break;
   }

   CRT_init(settings->delay, settings->colorScheme);

   Panel* panel = Panel_new(0, headerHeight, COLS, LINES - headerHeight - 2, PROCESS_CLASS, false, NULL);
   ProcessList_setPanel(pl, panel);
   
   if (sortKey > 0) {
      pl->sortKey = sortKey;
      pl->treeView = false;
      pl->direction = 1;
   }
   ProcessList_printHeader(pl, Panel_getHeader(panel));

   IncBuffer incSearch, incFilter;
   bool filtering = false;

   memset(&incSearch, 0, sizeof(IncBuffer));
   const char* searchFunctions[] = {"Next  ", "Cancel ", " Search: ", NULL};
   const char* searchKeys[] = {"F3", "Esc", "  "};
   int searchEvents[] = {KEY_F(3), 27, ERR};
   incSearch.bar = FunctionBar_new(searchFunctions, searchKeys, searchEvents);

   memset(&incFilter, 0, sizeof(IncBuffer));
   const char* filterFunctions[] = {"Done  ", "Clear ", " Filter: ", NULL};
   const char* filterKeys[] = {"Enter", "Esc", "  "};
   int filterEvents[] = {13, 27, ERR};
   incFilter.bar = FunctionBar_new(filterFunctions, filterKeys, filterEvents);
   
   IncBuffer* incMode = NULL;
   
   const char* defaultFunctions[] = {"Help  ", "Setup ", "Search", "Filter", "Tree  ",
       "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  ", NULL};
   FunctionBar* defaultBar = FunctionBar_new(defaultFunctions, NULL, NULL);

   ProcessList_scan(pl);
   usleep(75000);
   
   FunctionBar_draw(defaultBar, NULL);
   
   int acc = 0;
   bool follow = false;
 
   struct timeval tv;
   double newTime = 0.0;
   double oldTime = 0.0;
   bool recalculate;

   int ch = ERR;
   int closeTimeout = 0;

   while (!quit) {
      gettimeofday(&tv, NULL);
      newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);
      recalculate = (newTime - oldTime > CRT_delay);
      Process* p = (Process*)Panel_getSelected(panel);
      int following = (follow && p) ? p->pid : -1;
      if (recalculate)
         oldTime = newTime;
      if (doRefresh) {
         if (recalculate || doRecalculate) {
            ProcessList_scan(pl);
            doRecalculate = false;
         }
         if (refreshTimeout == 0 || pl->treeView) {
            ProcessList_sort(pl);
            refreshTimeout = 1;
         }
         ProcessList_rebuildPanel(pl, true, following, userOnly, userId, filtering, incFilter.buffer);
      }
      doRefresh = true;
      
      Header_draw(header);

      Panel_draw(panel, true);
      int prev = ch;
      if (incMode)
         move(LINES-1, CRT_cursorX);
      ch = getch();

      if (ch == ERR) {
         if (!incMode)
            refreshTimeout--;
         if (prev == ch && !recalculate) {
            closeTimeout++;
            if (closeTimeout == 100) {
               break;
            }
         } else
            closeTimeout = 0;
         continue;
      }

      if (incMode) {
         doRefresh = false;
         int size = Panel_size(panel);
         if (ch == KEY_F(3)) {
            if (Panel_size(panel) == 0) continue;
            int here = Panel_getSelectedIndex(panel);
            int i = here+1;
            while (i != here) {
               if (i == size)
                  i = 0;
               Process* p = (Process*) Panel_get(panel, i);
               if (String_contains_i(p->comm, incMode->buffer)) {
                  Panel_setSelected(panel, i);
                  break;
               }
               i++;
            }
            continue;
         } else if (isprint((char)ch) && (incMode->index < INCSEARCH_MAX)) {
            incMode->buffer[incMode->index] = ch;
            incMode->index++;
            incMode->buffer[incMode->index] = 0;
            if (incMode == &incFilter) {
               doRefresh = true;
               if (incFilter.index == 1) filtering = true;
            }
         } else if ((ch == KEY_BACKSPACE || ch == 127) && (incMode->index > 0)) {
            incMode->index--;
            incMode->buffer[incMode->index] = 0;
            if (incMode == &incFilter) {
               doRefresh = true;
               if (incFilter.index == 0) {
                  filtering = false;
                  IncBuffer_reset(incMode);
               }
            }
         } else {
            if (incMode == &incFilter) {
               doRefresh = true;
               if (ch == 27) {
                  filtering = false;
                  IncBuffer_reset(incMode);
               }
            }
            incMode = NULL;
            FunctionBar_draw(defaultBar, NULL);
            continue;
         }

         bool found = false;
         for (int i = 0; i < size; i++) {
            Process* p = (Process*) Panel_get(panel, i);
            if (String_contains_i(p->comm, incSearch.buffer)) {
               Panel_setSelected(panel, i);
               found = true;
               break;
            }
         }
         if (found)
            FunctionBar_draw(incMode->bar, incMode->buffer);
         else
            FunctionBar_drawAttr(incMode->bar, incMode->buffer, CRT_colors[FAILED_SEARCH]);
         continue;
      }
      if (isdigit((char)ch)) {
         if (Panel_size(panel) == 0) continue;
         pid_t pid = ch-48 + acc;
         for (int i = 0; i < ProcessList_size(pl) && ((Process*) Panel_getSelected(panel))->pid != pid; i++)
            Panel_setSelected(panel, i);
         acc = pid * 10;
         if (acc > 10000000)
            acc = 0;
         continue;
      } else {
         acc = 0;
      }

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
                  } else {
                     setSortKey(pl, field, panel, settings);
                  }
                  refreshTimeout = 0;
                  continue;
               } else if (mevent.y >= panel->y + 1 && mevent.y < LINES - 1) {
                  Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
                  doRefresh = false;
                  refreshTimeout = resetRefreshTimeout;
                  follow = true;
                  continue;
               } if (mevent.y == LINES - 1) {
                  FunctionBar* bar;
                  if (incMode) bar = incMode->bar;
                  else bar = defaultBar;
                  ch = FunctionBar_synthesizeEvent(bar, mevent.x);
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

      switch (ch) {
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-headerHeight-1);
         if (incMode)
            FunctionBar_draw(incMode->bar, incMode->buffer);
         else
            FunctionBar_draw(defaultBar, NULL);
         break;
      case 'M':
      {
         refreshTimeout = 0;
         setSortKey(pl, PERCENT_MEM, panel, settings);
         break;
      }
      case 'T':
      {
         refreshTimeout = 0;
         setSortKey(pl, TIME, panel, settings);
         break;
      }
      case 'U':
      {
         for (int i = 0; i < Panel_size(panel); i++) {
            Process* p = (Process*) Panel_get(panel, i);
            p->tag = false;
         }
         doRefresh = true;
         break;
      }
      case 'P':
      {
         refreshTimeout = 0;
         setSortKey(pl, PERCENT_CPU, panel, settings);
         break;
      }
      case KEY_F(1):
      case 'h':
      case '?':
      {
         showHelp(pl);
         FunctionBar_draw(defaultBar, NULL);
         refreshTimeout = 0;
         break;
      }
      case '\014': // Ctrl+L
      {
         clear();
         FunctionBar_draw(defaultBar, NULL);
         refreshTimeout = 0;
         break;
      }
      case ' ':
      {
         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         Process_toggleTag(p);
         Panel_onKey(panel, KEY_DOWN);
         break;
      }
      case 's':
      {
         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         TraceScreen* ts = TraceScreen_new(p);
         TraceScreen_run(ts);
         TraceScreen_delete(ts);
         clear();
         FunctionBar_draw(defaultBar, NULL);
         refreshTimeout = 0;
         CRT_enableDelay();
         break;
      }
      case 'l':
      {
         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         OpenFilesScreen* ts = OpenFilesScreen_new(p);
         OpenFilesScreen_run(ts);
         OpenFilesScreen_delete(ts);
         clear();
         FunctionBar_draw(defaultBar, NULL);
         refreshTimeout = 0;
         CRT_enableDelay();
         break;
      }
      case 'S':
      case 'C':
      case KEY_F(2):
      {
         Setup_run(settings, header);
         // TODO: shouldn't need this, colors should be dynamic
         ProcessList_printHeader(pl, Panel_getHeader(panel));
         headerHeight = Header_calculateHeight(header);
         Panel_move(panel, 0, headerHeight);
         Panel_resize(panel, COLS, LINES-headerHeight-1);
         FunctionBar_draw(defaultBar, NULL);
         refreshTimeout = 0;
         break;
      }
      case 'F':
      {
         follow = true;
         continue;
      }
      case 'u':
      {
         Panel* usersPanel = Panel_new(0, 0, 0, 0, LISTITEM_CLASS, true, ListItem_compare);
         Panel_setHeader(usersPanel, "Show processes of:");
         UsersTable_foreach(ut, addUserToVector, usersPanel);
         Vector_insertionSort(usersPanel->items);
         ListItem* allUsers = ListItem_new("All users", -1);
         Panel_insert(usersPanel, 0, (Object*) allUsers);
         const char* fuFunctions[] = {"Show    ", "Cancel ", NULL};
         ListItem* picked = (ListItem*) pickFromVector(panel, usersPanel, 20, headerHeight, fuFunctions, defaultBar, header);
         if (picked) {
            if (picked == allUsers) {
               userOnly = false;
            } else {
               setUserOnly(ListItem_getRef(picked), &userOnly, &userId);
            }
         }
         Panel_delete((Object*)usersPanel);
         break;
      }
      case '+':
      case '=':
      case '-':
      {
         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         p->showChildren = !p->showChildren;
         refreshTimeout = 0;
         doRecalculate = true;
         break;
      }
      case KEY_F(9):
      case 'k':
      {
         if (!killPanel) {
            killPanel = (Panel*) SignalsPanel_new(0, 0, 0, 0);
         }
         SignalsPanel_reset((SignalsPanel*) killPanel);
         const char* fuFunctions[] = {"Send  ", "Cancel ", NULL};
         ListItem* sgn = (ListItem*) pickFromVector(panel, killPanel, 15, headerHeight, fuFunctions, defaultBar, header);
         if (sgn) {
            if (sgn->key != 0) {
               Panel_setHeader(panel, "Sending...");
               Panel_draw(panel, true);
               refresh();
               foreachProcess(panel, (ForeachProcessFn) Process_sendSignal, (size_t) sgn->key, NULL);
               napms(500);
            }
         }
         ProcessList_printHeader(pl, Panel_getHeader(panel));
         refreshTimeout = 0;
         break;
      }
#if (HAVE_LIBHWLOC || HAVE_NATIVE_AFFINITY)
      case 'a':
      {
         if (pl->cpuCount == 1)
            break;

         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         Affinity* affinity = Process_getAffinity(p);
         if (!affinity) break;
         Panel* affinityPanel = AffinityPanel_new(pl, affinity);
         Affinity_delete(affinity);

         const char* fuFunctions[] = {"Set    ", "Cancel ", NULL};
         void* set = pickFromVector(panel, affinityPanel, 15, headerHeight, fuFunctions, defaultBar, header);
         if (set) {
            Affinity* affinity = AffinityPanel_getAffinity(affinityPanel);
            bool ok = foreachProcess(panel, (ForeachProcessFn) Process_setAffinity, (size_t) affinity, NULL);
            if (!ok) beep();
            Affinity_delete(affinity);
         }
         Panel_delete((Object*)affinityPanel);
         ProcessList_printHeader(pl, Panel_getHeader(panel));
         refreshTimeout = 0;
         break;
      }
#endif
      case KEY_F(10):
      case 'q':
         quit = 1;
         break;
      case '<':
      case ',':
      case KEY_F(18):
      case '>':
      case '.':
      case KEY_F(6):
      {
         Panel* sortPanel = Panel_new(0, 0, 0, 0, LISTITEM_CLASS, true, ListItem_compare);
         Panel_setHeader(sortPanel, "Sort by");
         const char* fuFunctions[] = {"Sort  ", "Cancel ", NULL};
         ProcessField* fields = pl->fields;
         for (int i = 0; fields[i]; i++) {
            char* name = String_trim(Process_fieldTitles[fields[i]]);
            Panel_add(sortPanel, (Object*) ListItem_new(name, fields[i]));
            if (fields[i] == pl->sortKey)
               Panel_setSelected(sortPanel, i);
            free(name);
         }
         ListItem* field = (ListItem*) pickFromVector(panel, sortPanel, 15, headerHeight, fuFunctions, defaultBar, header);
         if (field) {
            settings->changed = true;
            setSortKey(pl, field->key, panel, settings);
         } else {
            ProcessList_printHeader(pl, Panel_getHeader(panel));
         }
         ((Object*)sortPanel)->delete((Object*)sortPanel);
         refreshTimeout = 0;
         break;
      }
      case 'i':
      {
         Process* p = (Process*) Panel_getSelected(panel);
         if (!p) break;
         IOPriority ioprio = p->ioPriority;
         Panel* ioprioPanel = IOPriorityPanel_new(ioprio);
         const char* fuFunctions[] = {"Set    ", "Cancel ", NULL};
         void* set = pickFromVector(panel, ioprioPanel, 21, headerHeight, fuFunctions, defaultBar, header);
         if (set) {
            IOPriority ioprio = IOPriorityPanel_getIOPriority(ioprioPanel);
            bool ok = foreachProcess(panel, (ForeachProcessFn) Process_setIOPriority, (size_t) ioprio, NULL);
            if (!ok)
               beep();
         }
         Panel_delete((Object*)ioprioPanel);
         ProcessList_printHeader(pl, Panel_getHeader(panel));
         refreshTimeout = 0;
         break;
      }
      case 'I':
      {
         refreshTimeout = 0;
         settings->changed = true;
         ProcessList_invertSortOrder(pl);
         break;
      }
      case KEY_F(8):
      case '[':
      {
         doRefresh = changePriority(panel, 1);
         break;
      }
      case KEY_F(7):
      case ']':
      {
         doRefresh = changePriority(panel, -1);
         break;
      }
      case KEY_F(3):
      case '/':
         incMode = &incSearch;
         IncBuffer_reset(incMode);
         FunctionBar_draw(incSearch.bar, incSearch.buffer);
         break;
      case KEY_F(4):
      case '\\':
         incMode = &incFilter;
         refreshTimeout = 0;
         doRefresh = true;
         FunctionBar_draw(incFilter.bar, incFilter.buffer);
         continue;
      case 't':
      case KEY_F(5):
         refreshTimeout = 0;
         pl->treeView = !pl->treeView;
         if (pl->treeView) pl->direction = 1;
         ProcessList_printHeader(pl, Panel_getHeader(panel));
         ProcessList_expandTree(pl);
         settings->changed = true;
         if (following != -1) continue;
         break;
      case 'H':
         doRecalculate = true;
         refreshTimeout = 0;
         pl->hideUserlandThreads = !pl->hideUserlandThreads;
         pl->hideThreads = pl->hideUserlandThreads;
         settings->changed = true;
         break;
      case 'K':
         doRecalculate = true;
         refreshTimeout = 0;
         pl->hideKernelThreads = !pl->hideKernelThreads;
         settings->changed = true;
         break;
      default:
         doRefresh = false;
         refreshTimeout = resetRefreshTimeout;
         Panel_onKey(panel, ch);
         break;
      }
      follow = false;
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
   FunctionBar_delete((Object*)incFilter.bar);
   FunctionBar_delete((Object*)incSearch.bar);
   FunctionBar_delete((Object*)defaultBar);
   Panel_delete((Object*)panel);
   if (killPanel)
      ((Object*)killPanel)->delete((Object*)killPanel);
   UsersTable_delete(ut);
   Settings_delete(settings);
   if(pidWhiteList) {
      Hashtable_delete(pidWhiteList);
   }
   return 0;
}
