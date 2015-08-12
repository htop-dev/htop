/*
htop - htop.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "FunctionBar.h"
#include "Hashtable.h"
#include "ColumnsPanel.h"
#include "CRT.h"
#include "MainPanel.h"
#include "ProcessList.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "UsersTable.h"
#include "Platform.h"

#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//#link m

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

// ----------------------------------------

typedef struct CommandLineSettings_ {
   Hashtable* pidWhiteList;
   uid_t userId;
   int sortKey;
   int delay;
   bool useColors;
} CommandLineSettings;

static CommandLineSettings parseArguments(int argc, char** argv) {

   CommandLineSettings flags = {
      .pidWhiteList = NULL,
      .userId = -1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .useColors = true,
   };

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
      {"io",       no_argument,         0, 'i'},
      {0,0,0,0}
   };

   int opt, opti=0;
   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hvCs:d:u:p:i", long_opts, &opti))) {
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
               for (int j = 1; j < Platform_numberOfFields; j++) {
                  const char* name = Process_fields[j].name;
                  if (name) printf ("%s\n", name);
               }
               exit(0);
            }
            flags.sortKey = ColumnsPanel_fieldNameToIndex(optarg);
            if (flags.sortKey == -1) {
               fprintf(stderr, "Error: invalid column \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'd':
            if (sscanf(optarg, "%16d", &(flags.delay)) == 1) {
               if (flags.delay < 1) flags.delay = 1;
               if (flags.delay > 100) flags.delay = 100;
            } else {
               fprintf(stderr, "Error: invalid delay value \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'u':
            if (!Action_setUserOnly(optarg, &(flags.userId))) {
               fprintf(stderr, "Error: invalid user \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'C':
            flags.useColors = false;
            break;
         case 'p': {
            char* argCopy = strdup(optarg);
            char* saveptr;
            char* pid = strtok_r(argCopy, ",", &saveptr);

            if( !flags.pidWhiteList ) {
               flags.pidWhiteList = Hashtable_new(8, false);
            }

            while( pid ) {
                unsigned int num_pid = atoi(pid);
                Hashtable_put(flags.pidWhiteList, num_pid, (void *) 1);
                pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         default:
            exit(1);
      }
   }
   return flags;
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

int main(int argc, char** argv) {

   char *lc_ctype = getenv("LC_CTYPE");
   if(lc_ctype != NULL)
      setlocale(LC_CTYPE, lc_ctype);
   else if ((lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   CommandLineSettings flags = parseArguments(argc, argv); // may exit()

#ifdef HAVE_PROC
   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }
#endif
   
   Process_setupColumnWidths();
   
   UsersTable* ut = UsersTable_new();
   ProcessList* pl = ProcessList_new(ut, flags.pidWhiteList, flags.userId);
   
   Settings* settings = Settings_new(pl->cpuCount);
   pl->settings = settings;

   Header* header = Header_new(pl, settings, 2);

   Header_populateFromSettings(header);

   if (flags.delay != -1)
      settings->delay = flags.delay;
   if (!flags.useColors) 
      settings->colorScheme = COLORSCHEME_MONOCHROME;

   CRT_init(settings->delay, settings->colorScheme);
   
   MainPanel* panel = MainPanel_new();
   ProcessList_setPanel(pl, (Panel*) panel);

   MainPanel_updateTreeFunctions(panel, settings->treeView);
      
   if (flags.sortKey > 0) {
      settings->sortKey = flags.sortKey;
      settings->treeView = false;
      settings->direction = 1;
   }
   ProcessList_printHeader(pl, Panel_getHeader((Panel*)panel));

   State state = {
      .settings = settings,
      .ut = ut,
      .pl = pl,
      .panel = (Panel*) panel,
      .header = header,
   };
   MainPanel_setState(panel, &state);
   
   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, settings, true);
   ScreenManager_add(scr, (Panel*) panel, -1);

   ProcessList_scan(pl);
   millisleep(75);
   ProcessList_scan(pl);

   ScreenManager_run(scr, NULL, NULL);   
   
   /*
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
      int following = follow ? MainPanel_selectedPid((MainPanel*)panel) : -1;
      if (timeToRecalculate) {
         Header_draw(header);
         oldTime = newTime;
      }
      if (doRefresh) {
         if (timeToRecalculate || forceRecalculate) {
            ProcessList_scan(pl);
            forceRecalculate = false;
         }
         if (sortTimeout == 0 || settings->treeView) {
            ProcessList_sort(pl);
            sortTimeout = 1;
         }
         ProcessList_rebuildPanel(pl, true, following, IncSet_filter(inc));
         drawPanel = true;
      }
      doRefresh = true;
      
      if (settings->treeView) {
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
                  if (field == settings->sortKey) {
                     Settings_invertSortOrder(settings);
                     settings->treeView = false;
                     reaction |= HTOP_REDRAW_BAR;
                  } else {
                     reaction |= setSortKey(settings, field);
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
      
      if (ch < 255 && isdigit((char)ch)) {
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
         reaction |= (keys[ch])(&state);
      } else {
         doRefresh = false;
         sortTimeout = resetSortTimeout;
         Panel_onKey(panel, ch);
      }
      
      // Reaction handlers:

      if (reaction & HTOP_REDRAW_BAR) {
         updateTreeFunctions(defaultBar, settings->treeView);
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
   
   */
   
   attron(CRT_colors[RESET_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[RESET_COLOR]);
   refresh();
   
   CRT_done();
   if (settings->changed)
      Settings_write(settings);
   Header_delete(header);
   ProcessList_delete(pl);

   /*   
   FunctionBar_delete((Object*)defaultBar);
   Panel_delete((Object*)panel);
   */
   
   ScreenManager_delete(scr);
   
   UsersTable_delete(ut);
   Settings_delete(settings);
   
   if(flags.pidWhiteList) {
      Hashtable_delete(flags.pidWhiteList);
   }
   return 0;
}
