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
   fputs("htop " VERSION "\n", stdout);
   exit(0);
}

static void printHelpFlag() {
   fputs("htop " VERSION "\n"
         "Released under the GNU GPL.\n\n"
         "-C --no-color               Use a monochrome color scheme\n"
         "-m --no-mouse               Disable the mouse\n"
         "-d --delay=DELAY            Set the delay between updates, in tenths of seconds\n"
         "-h --help                   Print this help screen\n"
         "-s --sort-key=COLUMN        Sort by COLUMN (try --sort-key=help for a list)\n"
         "-t --tree                   Show the tree view by default\n"
         "-u --user[=USERNAME]        Show only processes for a given user (or $USER)\n"
         "-U --no-unicode             Do not use unicode but plain ASCII\n"
         "-p --pid=PID,[,PID,PID...]  Show only the given PIDs\n"
         "-v --version                Print version info\n"
         "\n"
         "Long options may be passed with a single dash.\n\n"
         "Press F1 inside htop for online help.\n"
         "See 'man htop' for more information.\n",
         stdout);
   exit(0);
}

// ----------------------------------------

typedef struct CommandLineSettings_ {
   Hashtable* pidMatchList;
   uid_t userId;
   int sortKey;
   int delay;
   bool useColors;
   bool enableMouse;
   bool treeView;
   bool allowUnicode;
} CommandLineSettings;

static CommandLineSettings parseArguments(int argc, char** argv) {

   CommandLineSettings flags = {
      .pidMatchList = NULL,
      .userId = -1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .useColors = true,
      .enableMouse = true,
      .treeView = false,
      .allowUnicode = true,
   };

   static struct option long_opts[] =
   {
      {"help",       no_argument,         0, 'h'},
      {"version",    no_argument,         0, 'v'},
      {"delay",      required_argument,   0, 'd'},
      {"sort-key",   required_argument,   0, 's'},
      {"user",       optional_argument,   0, 'u'},
      {"no-color",   no_argument,         0, 'C'},
      {"no-colour",  no_argument,         0, 'C'},
      {"no-mouse",   no_argument,         0, 'm'},
      {"no-unicode", no_argument,         0, 'U'},
      {"tree",       no_argument,         0, 't'},
      {"pid",        required_argument,   0, 'p'},
      {0,0,0,0}
   };

   int opt, opti=0;
   /* Parse arguments */
   while ((opt = getopt_long(argc, argv, "hvmCs:td:u::Up:", long_opts, &opti))) {
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
            if (!optarg && optind < argc && argv[optind] != NULL &&
                (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
               optarg = argv[optind++];
            }

            if (!optarg) {
               optarg = getenv("USER");
               flags.userId = geteuid();
            }

            if (!Action_setUserOnly(optarg, &(flags.userId))) {
               fprintf(stderr, "Error: invalid user \"%s\".\n", optarg);
               exit(1);
            }
            break;
         case 'C':
            flags.useColors = false;
            break;
         case 'm':
            flags.enableMouse = false;
            break;
         case 'U':
            flags.allowUnicode = false;
            break;
         case 't':
            flags.treeView = true;
            break;
         case 'p': {
            char* argCopy = xStrdup(optarg);
            char* saveptr;
            char* pid = strtok_r(argCopy, ",", &saveptr);

            if(!flags.pidMatchList) {
               flags.pidMatchList = Hashtable_new(8, false);
            }

            while(pid) {
                unsigned int num_pid = atoi(pid);
                Hashtable_put(flags.pidMatchList, num_pid, (void *) 1);
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

#ifdef HTOP_LINUX
   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }
#endif

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

   CRT_init(settings->delay, settings->colorScheme, flags.allowUnicode);

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

   attron(CRT_colors[RESET_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[RESET_COLOR]);
   refresh();

   CRT_done();
   if (settings->changed)
      Settings_write(settings);
   Header_delete(header);
   ProcessList_delete(pl);

   ScreenManager_delete(scr);

   UsersTable_delete(ut);
   Settings_delete(settings);

   if(flags.pidMatchList) {
      Hashtable_delete(flags.pidMatchList);
   }
   return 0;
}
