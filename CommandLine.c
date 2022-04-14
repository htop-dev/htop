/*
htop - CommandLine.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CommandLine.h"

#include <assert.h>
#include <ctype.h>
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
#include "DynamicColumn.h"
#include "DynamicMeter.h"
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


static void printVersionFlag(const char* name) {
   printf("%s " VERSION "\n", name);
}

static void printHelpFlag(const char* name) {
   printf("%s " VERSION "\n"
          COPYRIGHT "\n"
          "Released under the GNU GPLv2+.\n\n"
          "-C --no-color                   Use a monochrome color scheme\n"
          "-d --delay=DELAY                Set the delay between updates, in tenths of seconds\n"
          "-F --filter=FILTER              Show only the commands matching the given filter\n"
          "-h --help                       Print this help screen\n"
          "-H --highlight-changes[=DELAY]  Highlight new and old processes\n", name);
#ifdef HAVE_GETMOUSE
   printf("-M --no-mouse                   Disable the mouse\n");
#endif
   printf("-p --pid=PID[,PID,PID...]       Show only the given PIDs\n"
          "   --readonly                   Disable all system and process changing features\n"
          "-s --sort-key=COLUMN            Sort by COLUMN in list view (try --sort-key=help for a list)\n"
          "-t --tree                       Show the tree view (can be combined with -s)\n"
          "-u --user[=USERNAME]            Show only processes for a given user (or $USER)\n"
          "-U --no-unicode                 Do not use unicode but plain ASCII\n"
          "-V --version                    Print version info\n");
   Platform_longOptionsUsage(name);
   printf("\n"
          "Long options may be passed with a single dash.\n\n"
          "Press F1 inside %s for online help.\n"
          "See 'man %s' for more information.\n", name, name);
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
   bool readonly;
} CommandLineSettings;

static CommandLineStatus parseArguments(const char* program, int argc, char** argv, CommandLineSettings* flags) {

   *flags = (CommandLineSettings) {
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
      .readonly = false,
   };

   const struct option long_opts[] =
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
      {"readonly",   no_argument,         0, 128},
      PLATFORM_LONG_OPTIONS
      {0, 0, 0, 0}
   };

   int opt, opti = 0;
   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hVMCs:td:u::Up:F:H::", long_opts, &opti))) {
      if (opt == EOF)
         break;
      switch (opt) {
         case 'h':
            printHelpFlag(program);
            return STATUS_OK_EXIT;
         case 'V':
            printVersionFlag(program);
            return STATUS_OK_EXIT;
         case 's':
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            if (String_eq(optarg, "help")) {
               for (int j = 1; j < LAST_PROCESSFIELD; j++) {
                  const char* name = Process_fields[j].name;
                  const char* description = Process_fields[j].description;
                  if (name) printf("%19s %s\n", name, description);
               }
               return STATUS_OK_EXIT;
            }
            flags->sortKey = 0;
            for (int j = 1; j < LAST_PROCESSFIELD; j++) {
               if (Process_fields[j].name == NULL)
                  continue;
               if (String_eq(optarg, Process_fields[j].name)) {
                  flags->sortKey = j;
                  break;
               }
            }
            if (flags->sortKey == 0) {
               fprintf(stderr, "Error: invalid column \"%s\".\n", optarg);
               return STATUS_ERROR_EXIT;
            }
            break;
         case 'd':
            if (sscanf(optarg, "%16d", &(flags->delay)) == 1) {
               if (flags->delay < 1) flags->delay = 1;
               if (flags->delay > 100) flags->delay = 100;
            } else {
               fprintf(stderr, "Error: invalid delay value \"%s\".\n", optarg);
               return STATUS_ERROR_EXIT;
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
               flags->userId = geteuid();
            } else if (!Action_setUserOnly(username, &(flags->userId))) {
               for (const char *itr = username; *itr; ++itr)
                  if (!isdigit((unsigned char)*itr)) {
                     fprintf(stderr, "Error: invalid user \"%s\".\n", username);
                     return STATUS_ERROR_EXIT;
                  }
               flags->userId = atol(username);
            }
            break;
         }
         case 'C':
            flags->useColors = false;
            break;
         case 'M':
#ifdef HAVE_GETMOUSE
            flags->enableMouse = false;
#endif
            break;
         case 'U':
            flags->allowUnicode = false;
            break;
         case 't':
            flags->treeView = true;
            break;
         case 'p': {
            assert(optarg); /* please clang analyzer, cause optarg can be NULL in the 'u' case */
            char* argCopy = xStrdup(optarg);
            char* saveptr;
            const char* pid = strtok_r(argCopy, ",", &saveptr);

            if (!flags->pidMatchList) {
               flags->pidMatchList = Hashtable_new(8, false);
            }

            while(pid) {
                unsigned int num_pid = atoi(pid);
                //  deepcode ignore CastIntegerToAddress: we just want a non-NULL pointer here
                Hashtable_put(flags->pidMatchList, num_pid, (void *) 1);
                pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         case 'F': {
            assert(optarg);
            free_and_xStrdup(&flags->commFilter, optarg);
            break;
         }
         case 'H': {
            const char *delay = optarg;
            if (!delay && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
                delay = argv[optind++];
            }
            if (delay) {
                if (sscanf(delay, "%16d", &(flags->highlightDelaySecs)) == 1) {
                   if (flags->highlightDelaySecs < 1)
                      flags->highlightDelaySecs = 1;
                } else {
                   fprintf(stderr, "Error: invalid highlight delay value \"%s\".\n", delay);
                   return STATUS_ERROR_EXIT;
                }
            }
            flags->highlightChanges = true;
            break;
         }
         case 128:
            flags->readonly = true;
            break;

         default: {
            CommandLineStatus status;
            if ((status = Platform_getLongOption(opt, argc, argv)) != STATUS_OK)
               return status;
            break;
         }
      }
   }
   return STATUS_OK;
}

static void CommandLine_delay(ProcessList* pl, unsigned long millisec) {
   struct timespec req = {
      .tv_sec = 0,
      .tv_nsec = millisec * 1000000L
   };
   while (nanosleep(&req, &req) == -1)
      continue;
   Platform_gettime_realtime(&pl->realtime, &pl->realtimeMs);
}

static void setCommFilter(State* state, char** commFilter) {
   ProcessList* pl = state->pl;
   IncSet* inc = state->mainPanel->inc;

   IncSet_setFilter(inc, *commFilter);
   pl->incFilter = IncSet_filter(inc);

   free(*commFilter);
   *commFilter = NULL;
}

int CommandLine_run(const char* name, int argc, char** argv) {

   /* initialize locale */
   const char* lc_ctype;
   if ((lc_ctype = getenv("LC_CTYPE")) || (lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   CommandLineStatus status = STATUS_OK;
   CommandLineSettings flags = { 0 };

   if ((status = parseArguments(name, argc, argv, &flags)) != STATUS_OK)
      return status != STATUS_OK_EXIT ? 1 : 0;

   if (flags.readonly)
      Settings_enableReadonly();

   if (!Platform_init())
      return 1;

   Process_setupColumnWidths();

   UsersTable* ut = UsersTable_new();
   Hashtable* dc = DynamicColumns_new();
   Hashtable* dm = DynamicMeters_new();
   if (!dc)
      dc = Hashtable_new(0, true);

   ProcessList* pl = ProcessList_new(ut, dm, dc, flags.pidMatchList, flags.userId);

   Settings* settings = Settings_new(pl->activeCPUs, dc);
   pl->settings = settings;

   Header* header = Header_new(pl, settings, 2);

   Header_populateFromSettings(header);

   if (flags.delay != -1)
      settings->delay = flags.delay;
   if (!flags.useColors)
      settings->colorScheme = COLORSCHEME_MONOCHROME;
#ifdef HAVE_GETMOUSE
   if (!flags.enableMouse)
      settings->enableMouse = false;
#endif
   if (flags.treeView)
      settings->ss->treeView = true;
   if (flags.highlightChanges)
      settings->highlightChanges = true;
   if (flags.highlightDelaySecs != -1)
      settings->highlightDelaySecs = flags.highlightDelaySecs;
   if (flags.sortKey > 0) {
      // -t -s <key> means "tree sorted by key"
      // -s <key> means "list sorted by key" (previous existing behavior)
      if (!flags.treeView) {
         settings->ss->treeView = false;
      }
      ScreenSettings_setSortKey(settings->ss, flags.sortKey);
   }

   CRT_init(settings, flags.allowUnicode);

   MainPanel* panel = MainPanel_new();
   ProcessList_setPanel(pl, (Panel*) panel);

   MainPanel_updateLabels(panel, settings->ss->treeView, flags.commFilter);

   State state = {
      .settings = settings,
      .ut = ut,
      .pl = pl,
      .mainPanel = panel,
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
   CommandLine_delay(pl, 75);
   ProcessList_scan(pl, false);

   if (settings->ss->allBranchesCollapsed)
      ProcessList_collapseAllBranches(pl);

   ScreenManager_run(scr, NULL, NULL, NULL);

   Platform_done();

   CRT_done();

   if (settings->changed) {
      int r = Settings_write(settings, false);
      if (r < 0)
         fprintf(stderr, "Can not save configuration to %s: %s\n", settings->filename, strerror(-r));
   }

   Header_delete(header);
   ProcessList_delete(pl);

   ScreenManager_delete(scr);
   MetersPanel_cleanup();

   UsersTable_delete(ut);

   if (flags.pidMatchList)
      Hashtable_delete(flags.pidMatchList);

   CRT_resetSignalHandlers();

   /* Delete these last, since they can get accessed in the crash handler */
   Settings_delete(settings);
   DynamicColumns_delete(dc);
   DynamicMeters_delete(dm);

   return 0;
}
