/*
htop - htop.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <assert.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "Action.h"
#include "CRT.h"
#include "Hashtable.h"
#include "Header.h"
#include "IncSet.h"
#include "MainPanel.h"
#include "MetersPanel.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "UsersTable.h"
#include "XUtils.h"

static void printVersionFlag(void) {
   fputs(PACKAGE " " VERSION "\n", stdout);
}

static void printHelpFlag(void) {
   fputs(PACKAGE " " VERSION "\n"
         COPYRIGHT "\n"
         "Released under the GNU GPLv2.\n\n"
         "-C --no-color                   Use a monochrome color scheme\n"
         "-d --delay=DELAY                Set the delay between updates, in tenths of seconds\n"
         "-F --filter=FILTER              Show only the commands matching the given filter\n"
         "-h --help                       Print this help screen\n"
         "-H --highlight-changes[=DELAY]  Highlight new and old processes\n"
         "-M --no-mouse                   Disable the mouse\n"
         "-p --pid=PID[,PID,PID...]       Show only the given PIDs\n"
         "-s --sort-key=COLUMN            Sort by COLUMN (try --sort-key=help for a list)\n"
         "-t --tree                       Show the tree view by default\n"
         "-u --user[=USERNAME]            Show only processes for a given user (or $USER)\n"
         "-U --no-unicode                 Do not use unicode but plain ASCII\n"
         "-V --version                    Print version info\n"
         "\n"
         "Long options may be passed with a single dash.\n\n"
         "Press F1 inside " PACKAGE " for online help.\n"
         "See 'man " PACKAGE "' for more information.\n",
         stdout);
}

// ----------------------------------------

typedef struct CommandLineSettings_ {
   Hashtable* pidMatchList;
   char* commFilter;
   uid_t userId;
   int sortKey;
   int delay;
   bool useColors;
   bool enableMouse;
   bool treeView;
   bool allowUnicode;
   bool highlightChanges;
   int highlightDelaySecs;
} CommandLineSettings;

static CommandLineSettings parseArguments(int argc, char** argv) {

   CommandLineSettings flags = {
      .pidMatchList = NULL,
      .commFilter = NULL,
      .userId = (uid_t)-1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .useColors = true,
      .enableMouse = true,
      .treeView = false,
      .allowUnicode = true,
      .highlightChanges = false,
      .highlightDelaySecs = -1,
   };

   static struct option long_opts[] =
   {
      {"help",       no_argument,         0, 'h'},
      {"version",    no_argument,         0, 'V'},
      {"delay",      required_argument,   0, 'd'},
      {"sort-key",   required_argument,   0, 's'},
      {"user",       optional_argument,   0, 'u'},
      {"no-color",   no_argument,         0, 'C'},
      {"no-colour",  no_argument,         0, 'C'},
      {"no-mouse",   no_argument,         0, 'M'},
      {"no-unicode", no_argument,         0, 'U'},
      {"tree",       no_argument,         0, 't'},
      {"pid",        required_argument,   0, 'p'},
      {"filter",     required_argument,   0, 'F'},
      {"highlight-changes", optional_argument, 0, 'H'},
      {0,0,0,0}
   };

   int opt, opti=0;
   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hVMCs:td:u::Up:F:H::", long_opts, &opti))) {
      if (opt == EOF) break;
      switch (opt) {
         case 'h':
            printHelpFlag();
            exit(0);
         case 'V':
            printVersionFlag();
            exit(0);
         case 's':
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            if (String_eq(optarg, "help")) {
               for (int j = 1; j < Platform_numberOfFields; j++) {
                  const char* name = Process_fields[j].name;
                  if (name) printf ("%s\n", name);
               }
               exit(0);
            }
            flags.sortKey = 0;
            for (int j = 1; j < Platform_numberOfFields; j++) {
               if (Process_fields[j].name == NULL)
                  continue;
               if (String_eq(optarg, Process_fields[j].name)) {
                  flags.sortKey = j;
                  break;
               }
            }
            if (flags.sortKey == 0) {
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
         {
            const char *username = optarg;
            if (!username && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
               username = argv[optind++];
            }

            if (!username) {
               flags.userId = geteuid();
            } else if (!Action_setUserOnly(username, &(flags.userId))) {
               fprintf(stderr, "Error: invalid user \"%s\".\n", username);
               exit(1);
            }
            break;
         }
         case 'C':
            flags.useColors = false;
            break;
         case 'M':
            flags.enableMouse = false;
            break;
         case 'U':
            flags.allowUnicode = false;
            break;
         case 't':
            flags.treeView = true;
            break;
         case 'p': {
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            char* argCopy = xStrdup(optarg);
            char* saveptr;
            char* pid = strtok_r(argCopy, ",", &saveptr);

            if(!flags.pidMatchList) {
               flags.pidMatchList = Hashtable_new(8, false);
            }

            while(pid) {
                unsigned int num_pid = atoi(pid);
                //  deepcode ignore CastIntegerToAddress: we just want a non-NUll pointer here
                Hashtable_put(flags.pidMatchList, num_pid, (void *) 1);
                pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         case 'F': {
            assert(optarg);
            flags.commFilter = xStrdup(optarg);

            break;
         }
         case 'H': {
            const char *delay = optarg;
            if (!delay && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
                delay = argv[optind++];
            }
            if (delay) {
                if (sscanf(delay, "%16d", &(flags.highlightDelaySecs)) == 1) {
                   if (flags.highlightDelaySecs < 1)
                      flags.highlightDelaySecs = 1;
                } else {
                   fprintf(stderr, "Error: invalid highlight delay value \"%s\".\n", delay);
                   exit(1);
                }
            }
            flags.highlightChanges = true;
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

static void setCommFilter(State* state, char** commFilter) {
   MainPanel* panel = (MainPanel*)state->panel;
   ProcessList* pl = state->pl;
   IncSet* inc = panel->inc;
   size_t maxlen = sizeof(inc->modes[INC_FILTER].buffer) - 1;
   char* buffer = inc->modes[INC_FILTER].buffer;

   strncpy(buffer, *commFilter, maxlen);
   buffer[maxlen] = 0;
   inc->modes[INC_FILTER].index = strlen(buffer);
   inc->filtering = true;
   pl->incFilter = IncSet_filter(inc);

   free(*commFilter);
   *commFilter = NULL;
}

int main(int argc, char** argv) {

   /* initialize locale */
   const char* lc_ctype;
   if ((lc_ctype = getenv("LC_CTYPE")) || (lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   CommandLineSettings flags = parseArguments(argc, argv);

   Platform_init();

   Process_setupColumnWidths();

   UsersTable* ut = UsersTable_new();
   ProcessList* pl = ProcessList_new(ut, flags.pidMatchList, flags.userId);

   Settings* settings = Settings_new(pl->cpuCount);
   pl->settings = settings;

   Header* header = Header_new(pl, settings, 2);

   Header_populateFromSettings(header);

   if (flags.delay != -1)
      settings->delay = flags.delay;
   if (!flags.useColors)
      settings->colorScheme = COLORSCHEME_MONOCHROME;
   if (!flags.enableMouse)
      settings->enableMouse = false;
   if (flags.treeView)
      settings->treeView = true;
   if (flags.highlightChanges)
      settings->highlightChanges = true;
   if (flags.highlightDelaySecs != -1)
      settings->highlightDelaySecs = flags.highlightDelaySecs;
   if (flags.sortKey > 0) {
      settings->sortKey = flags.sortKey;
      settings->treeView = false;
      settings->direction = 1;
   }

   CRT_init(&(settings->delay), settings->colorScheme, flags.allowUnicode);

   MainPanel* panel = MainPanel_new();
   ProcessList_setPanel(pl, (Panel*) panel);

   MainPanel_updateTreeFunctions(panel, settings->treeView);

   ProcessList_printHeader(pl, Panel_getHeader((Panel*)panel));

   State state = {
      .settings = settings,
      .ut = ut,
      .pl = pl,
      .panel = (Panel*) panel,
      .header = header,
      .pauseProcessUpdate = false,
      .hideProcessSelection = false,
   };

   MainPanel_setState(panel, &state);
   if (flags.commFilter)
      setCommFilter(&state, &(flags.commFilter));

   ScreenManager* scr = ScreenManager_new(header, settings, &state, true);
   ScreenManager_add(scr, (Panel*) panel, -1);

   ProcessList_scan(pl, false);
   millisleep(75);
   ProcessList_scan(pl, false);

   ScreenManager_run(scr, NULL, NULL);

   attron(CRT_colors[RESET_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[RESET_COLOR]);
   refresh();

   Platform_done();

   CRT_done();
   if (settings->changed)
      Settings_write(settings);
   Header_delete(header);
   ProcessList_delete(pl);

   ScreenManager_delete(scr);
   MetersPanel_cleanup();

   UsersTable_delete(ut);
   Settings_delete(settings);

   if (flags.pidMatchList)
      Hashtable_delete(flags.pidMatchList);

   return 0;
}
