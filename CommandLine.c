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
#include "DynamicScreen.h"
#include "Hashtable.h"
#include "Header.h"
#include "IncSet.h"
#include "Machine.h"
#include "MainPanel.h"
#include "MetersPanel.h"
#include "Panel.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessTable.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "Table.h"
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
   printf("-n --max-iterations=NUMBER      Exit htop after NUMBER iterations/frame updates\n"
          "-p --pid=PID[,PID,PID...]       Show only the given PIDs\n"
          "   --readonly                   Disable all system and process changing features\n"
          "-s --sort-key=COLUMN            Sort by COLUMN in list view (try --sort-key=help for a list)\n"
          "-t --tree                       Show the tree view (can be combined with -s)\n"
          "-u --user[=USERNAME]            Show only processes for a given user (or $USER)\n"
          "-U --no-unicode                 Do not use unicode but plain ASCII\n"
          "-V --version                    Print version info\n");
   Platform_longOptionsUsage(name);
   printf("\n"
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
   int iterationsRemaining;
   bool useColors;
#ifdef HAVE_GETMOUSE
   bool enableMouse;
#endif
   bool treeView;
   bool allowUnicode;
   bool highlightChanges;
   int highlightDelaySecs;
   bool readonly;
} CommandLineSettings;

static CommandLineStatus parseArguments(int argc, char** argv, CommandLineSettings* flags) {

   *flags = (CommandLineSettings) {
      .pidMatchList = NULL,
      .commFilter = NULL,
      .userId = (uid_t)-1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .iterationsRemaining = -1,
      .useColors = true,
#ifdef HAVE_GETMOUSE
      .enableMouse = true,
#endif
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
      {"max-iterations", required_argument, 0, 'n'},
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
   while ((opt = getopt_long(argc, argv, "hVMCs:td:n:u::Up:F:H::", long_opts, &opti))) {
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
                  if (name)
                     printf("%19s %s\n", name, description);
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
               if (flags->delay < 1)
                  flags->delay = 1;
               if (flags->delay > 100)
                  flags->delay = 100;
            } else {
               fprintf(stderr, "Error: invalid delay value \"%s\".\n", optarg);
               return STATUS_ERROR_EXIT;
            }
            break;
         case 'n':
            if (sscanf(optarg, "%16d", &flags->iterationsRemaining) == 1) {
               if (flags->iterationsRemaining <= 0) {
                  fprintf(stderr, "Error: maximum iteration count must be positive.\n");
                  return STATUS_ERROR_EXIT;
               }
            } else {
               fprintf(stderr, "Error: invalid maximum iteration count \"%s\".\n", optarg);
               return STATUS_ERROR_EXIT;
            }
            break;
         case 'u': {
            const char* username = optarg;
            if (!username && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
               username = argv[optind++];
            }

            if (!username) {
               flags->userId = geteuid();
            } else if (!Action_setUserOnly(username, &(flags->userId))) {
               for (const char* itr = username; *itr; ++itr)
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

            while (pid) {
               unsigned int num_pid = atoi(pid);
               //  deepcode ignore CastIntegerToAddress: we just want a non-NULL pointer here
               Hashtable_put(flags->pidMatchList, num_pid, (void*) 1);
               pid = strtok_r(NULL, ",", &saveptr);
            }
            free(argCopy);

            break;
         }
         case 'F':
            assert(optarg);
            free_and_xStrdup(&flags->commFilter, optarg);
            break;
         case 'H': {
            const char* delay = optarg;
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

   if (optind < argc) {
      fprintf(stderr, "Error: unsupported non-option ARGV-elements:");
      while (optind < argc)
         fprintf(stderr, " %s", argv[optind++]);
      fprintf(stderr, "\n");
      return STATUS_ERROR_EXIT;
   }

   return STATUS_OK;
}

static void CommandLine_delay(Machine* host, unsigned long millisec) {
   struct timespec req = {
      .tv_sec = 0,
      .tv_nsec = millisec * 1000000L
   };
   while (nanosleep(&req, &req) == -1)
      continue;
   Platform_gettime_realtime(&host->realtime, &host->realtimeMs);
}

static void setCommFilter(State* state, char** commFilter) {
   Table* table = state->host->activeTable;
   IncSet* inc = state->mainPanel->inc;

   IncSet_setFilter(inc, *commFilter);
   table->incFilter = IncSet_filter(inc);

   free(*commFilter);
   *commFilter = NULL;
}

int CommandLine_run(int argc, char** argv) {

   /* initialize locale */
   const char* lc_ctype;
   if ((lc_ctype = getenv("LC_CTYPE")) || (lc_ctype = getenv("LC_ALL")))
      setlocale(LC_CTYPE, lc_ctype);
   else
      setlocale(LC_CTYPE, "");

   CommandLineStatus status = STATUS_OK;
   CommandLineSettings flags = { 0 };

   if ((status = parseArguments(argc, argv, &flags)) != STATUS_OK)
      return status != STATUS_OK_EXIT ? 1 : 0;

   if (flags.readonly)
      Settings_enableReadonly();

   if (!Platform_init())
      return 1;

   UsersTable* ut = UsersTable_new();
   Hashtable* dm = DynamicMeters_new();
   Hashtable* dc = DynamicColumns_new();
   Hashtable* ds = DynamicScreens_new();

   Machine* host = Machine_new(ut, flags.userId);
   ProcessTable* pt = ProcessTable_new(host, flags.pidMatchList);
   Settings* settings = Settings_new(host->activeCPUs, dm, dc, ds);
   Machine_populateTablesFromSettings(host, settings, &pt->super);

   Header* header = Header_new(host, 2);
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

   host->iterationsRemaining = flags.iterationsRemaining;
   CRT_init(settings, flags.allowUnicode, flags.iterationsRemaining != -1);

   MainPanel* panel = MainPanel_new();
   Machine_setTablesPanel(host, (Panel*) panel);

   MainPanel_updateLabels(panel, settings->ss->treeView, flags.commFilter);

   State state = {
      .host = host,
      .mainPanel = panel,
      .header = header,
      .pauseUpdate = false,
      .hideSelection = false,
      .hideMeters = false,
   };

   MainPanel_setState(panel, &state);
   if (flags.commFilter)
      setCommFilter(&state, &(flags.commFilter));

   ScreenManager* scr = ScreenManager_new(header, host, &state, true);
   ScreenManager_add(scr, (Panel*) panel, -1);

   Machine_scan(host);
   Machine_scanTables(host);
   CommandLine_delay(host, 75);
   Machine_scan(host);
   Machine_scanTables(host);

   if (settings->ss->allBranchesCollapsed)
      Table_collapseAllBranches(&pt->super);

   ScreenManager_run(scr, NULL, NULL, NULL);

   Platform_done();

   CRT_done();

   if (settings->changed) {
      int r = Settings_write(settings, false);
      if (r < 0)
         fprintf(stderr, "Can not save configuration to %s: %s\n", settings->filename, strerror(-r));
   }

   Header_delete(header);
   Machine_delete(host);

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
   DynamicScreens_delete(ds);

   return 0;
}
