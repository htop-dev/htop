/*
htop - htop.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <math.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdbool.h>

#include "ProcessList.h"
#include "CRT.h"
#include "ListBox.h"
#include "UsersTable.h"
#include "SignalItem.h"
#include "RichString.h"
#include "Settings.h"
#include "ScreenManager.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "CategoriesListBox.h"
#include "SignalsListBox.h"
#include "TraceScreen.h"

#include "config.h"
#include "debug.h"

//#link m

#define INCSEARCH_MAX 40

/* private property */
char htop_barCharacters[] = "|#*@$%&";

void printVersionFlag() {
   clear();
   printf("htop " VERSION " - (C) 2004-2006 Hisham Muhammad.\n");
   printf("Released under the GNU GPL.\n\n");
   exit(0);
}

void printHelpFlag() {
   clear();
   printf("htop " VERSION " - (C) 2004-2006 Hisham Muhammad.\n");
   printf("Released under the GNU GPL.\n\n");
   printf("-d DELAY     Delay between updates, in tenths of seconds\n\n");
   printf("-u USERNAME  Show only processes of a given user\n\n");
   printf("Press F1 inside htop for online help.\n");
   printf("See the man page for full information.\n\n");
   exit(0);
}

void showHelp() {
   clear();
   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(0, 0, "htop " VERSION " - (C) 2004-2006 Hisham Muhammad.");
   mvaddstr(1, 0, "Released under the GNU GPL. See 'man' page for more info.");

   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(3, 0, "CPU usage bar: ");
   #define addattrstr(a,s) attrset(a);addstr(s)
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[CPU_NICE], "low-priority"); addstr("/");
   addattrstr(CRT_colors[CPU_NORMAL], "normal"); addstr("/");
   addattrstr(CRT_colors[CPU_KERNEL], "kernel");
   addattrstr(CRT_colors[BAR_SHADOW], "      used%");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(4, 0, "Memory bar:    ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[MEMORY_USED], "used"); addstr("/");
   addattrstr(CRT_colors[MEMORY_BUFFERS], "buffers"); addstr("/");
   addattrstr(CRT_colors[MEMORY_CACHE], "cache");
   addattrstr(CRT_colors[BAR_SHADOW], "         used/total");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(5, 0, "Swap bar:      ");
   addattrstr(CRT_colors[BAR_BORDER], "[");
   addattrstr(CRT_colors[SWAP], "used");
   addattrstr(CRT_colors[BAR_SHADOW], "                       used/total");
   addattrstr(CRT_colors[BAR_BORDER], "]");
   attrset(CRT_colors[DEFAULT_COLOR]);
   mvaddstr(6,0, "Type and layout of header meters is configurable in the setup screen.");

   mvaddstr( 8, 0, " Arrows: scroll process list             F5 t: tree view");
   mvaddstr( 9, 0, " Digits: incremental PID search             u: show processes of a single user");
   mvaddstr(10, 0, "   F3 /: incremental name search            H: hide/show user threads");
   mvaddstr(11, 0, "                                            K: hide/show kernel threads");
   mvaddstr(12, 0, "  Space: tag processes                      F: cursor follows process");
   mvaddstr(13, 0, "      U: untag all processes");
   mvaddstr(14, 0, "   F9 k: kill process/tagged processes      P: sort by CPU%");
   mvaddstr(15, 0, " + [ F7: lower priority (+ nice)            M: sort by MEM%");
   mvaddstr(16, 0, " - ] F8: higher priority (root only)        T: sort by TIME");
   mvaddstr(17, 0, "                                         F4 I: invert sort order");
   mvaddstr(18, 0, "   F2 S: setup                           F6 >: select sort column");
   mvaddstr(19, 0, "   F1 h: show this help screen");
   mvaddstr(20, 0, "  F10 q: quit                               s: trace syscalls with strace");

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr( 8, 0, " Arrows"); mvaddstr( 8,40, " F5 t");
   mvaddstr( 9, 0, " Digits"); mvaddstr( 9,40, "    u");
   mvaddstr(10, 0, "   F3 /"); mvaddstr(10,40, "    H");
                               mvaddstr(11,40, "    K");
   mvaddstr(12, 0, "  Space"); mvaddstr(12,40, "    F");
   mvaddstr(13, 0, "      U");
   mvaddstr(14, 0, "   F9 k"); mvaddstr(14,40, "    P");
   mvaddstr(15, 0, " + [ F7"); mvaddstr(15,40, "    M");
   mvaddstr(16, 0, " - ] F8"); mvaddstr(16,40, "    T");
                               mvaddstr(17,40, " F4 I");
   mvaddstr(18, 0, "   F2 S"); mvaddstr(18,40, " F6 >");
   mvaddstr(19, 0, "   F1 h");
   mvaddstr(20, 0, "  F10 q"); mvaddstr(20,40, "    s");
   attrset(CRT_colors[DEFAULT_COLOR]);

   attrset(CRT_colors[HELP_BOLD]);
   mvaddstr(23,0, "Press any key to return.");
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
   CRT_readKey();
   clear();
}

static void Setup_run(Settings* settings, int headerHeight) {
   ScreenManager* scr = ScreenManager_new(0, headerHeight, 0, -1, HORIZONTAL, true);
   CategoriesListBox* lbCategories = CategoriesListBox_new(settings, scr);
   ScreenManager_add(scr, (ListBox*) lbCategories, NULL, 16);
   CategoriesListBox_makeMetersPage(lbCategories);
   ListBox* lbFocus;
   int ch;
   ScreenManager_run(scr, &lbFocus, &ch);
   ScreenManager_delete(scr);
}

static bool changePriority(ListBox* lb, int delta) {
   bool anyTagged = false;
   for (int i = 0; i < ListBox_getSize(lb); i++) {
      Process* p = (Process*) ListBox_get(lb, i);
      if (p->tag) {
         Process_setPriority(p, p->nice + delta);
         anyTagged = true;
      }
   }
   if (!anyTagged) {
      Process* p = (Process*) ListBox_getSelected(lb);
      Process_setPriority(p, p->nice + delta);
   }
   return anyTagged;
}

static HandlerResult pickWithEnter(ListBox* lb, int ch) {
   if (ch == 13)
      return BREAK_LOOP;
   return IGNORED;
}

static Object* pickFromList(ListBox* lb, ListBox* list, int x, int y, char** keyLabels, FunctionBar* prevBar) {
   char* fuKeys[2] = {"Enter", "Esc"};
   int fuEvents[2] = {13, 27};
   if (!lb->eventHandler)
      ListBox_setEventHandler(list, pickWithEnter);
   ScreenManager* scr = ScreenManager_new(0, y, 0, -1, HORIZONTAL, false);
   ScreenManager_add(scr, list, FunctionBar_new(2, keyLabels, fuKeys, fuEvents), x - 1);
   ScreenManager_add(scr, lb, NULL, -1);
   ListBox* lbFocus;
   int ch;
   ScreenManager_run(scr, &lbFocus, &ch);
   ScreenManager_delete(scr);
   ListBox_move(lb, 0, y);
   ListBox_resize(lb, COLS, LINES-y-1);
   FunctionBar_draw(prevBar, NULL);
   if (lbFocus == list && ch == 13) {
      return ListBox_getSelected(list);
   }
   return NULL;
}

void addUserToList(int key, void* userCast, void* lbCast) {
   char* user = (char*) userCast;
   ListBox* lb = (ListBox*) lbCast;
   ListBox_add(lb, (Object*) ListItem_new(user, key));
}

void setUserOnly(const char* userName, bool* userOnly, uid_t* userId) {
   struct passwd* user = getpwnam(userName);
   if (user) {
      *userOnly = true;
      *userId = user->pw_uid;
   }
}

int main(int argc, char** argv) {

   int delay = -1;
   bool userOnly = false;
   uid_t userId = 0;

   if (argc > 0) {
      if (String_eq(argv[1], "--help")) {
         printHelpFlag();
      } else if (String_eq(argv[1], "--version")) {
         printVersionFlag();
      } else if (String_eq(argv[1], "-d")) {
         if (argc < 2) printHelpFlag();
         sscanf(argv[2], "%d", &delay);
         if (delay < 1) delay = 1;
         if (delay > 100) delay = 100;
      } else if (String_eq(argv[1], "-u")) {
         if (argc < 2) printHelpFlag();
         setUserOnly(argv[2], &userOnly, &userId);
      }
   }
   
   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }

   ListBox* lb;
   int quit = 0;
   int refreshTimeout = 0;
   int resetRefreshTimeout = 5;
   bool doRefresh = true;
   Settings* settings;
   
   ListBox* lbk = NULL;

   char incSearchBuffer[INCSEARCH_MAX];
   int incSearchIndex = 0;
   incSearchBuffer[0] = 0;
   bool incSearchMode = false;

   ProcessList* pl = NULL;
   UsersTable* ut = UsersTable_new();

   pl = ProcessList_new(ut);
   
   Header* header = Header_new(pl);
   settings = Settings_new(pl, header);
   int headerHeight = Header_calculateHeight(header);

   // FIXME: move delay code to settings
   if (delay != -1)
      settings->delay = delay;
   
   CRT_init(settings->delay, settings->colorScheme);
   
   lb = ListBox_new(0, headerHeight, COLS, LINES - headerHeight - 2, PROCESS_CLASS, false);
   ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
   
   char* searchFunctions[3] = {"Next  ", "Exit  ", " Search: "};
   char* searchKeys[3] = {"F3", "Esc", "  "};
   int searchEvents[3] = {KEY_F(3), 27, ERR};
   FunctionBar* searchBar = FunctionBar_new(3, searchFunctions, searchKeys, searchEvents);
   
   char* defaultFunctions[10] = {"Help  ", "Setup ", "Search", "Invert", "Tree  ",
       "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  "};
   FunctionBar* defaultBar = FunctionBar_new(10, defaultFunctions, NULL, NULL);

   ProcessList_scan(pl);
   usleep(75000);
   
   FunctionBar_draw(defaultBar, NULL);
   
   int acc = 0;
   bool follow = false;
 
   struct timeval tv;
   double newTime = 0.0;
   double oldTime = 0.0;
   bool recalculate;

   int ch = 0;
   int closeTimeout = 0;

   while (!quit) {
      gettimeofday(&tv, NULL);
      newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);
      recalculate = (newTime - oldTime > CRT_delay);
      if (recalculate)
         oldTime = newTime;
      if (doRefresh) {
         incSearchIndex = 0;
         incSearchBuffer[0] = 0;
         int currPos = ListBox_getSelectedIndex(lb);
         int currPid = 0;
         int currScrollV = lb->scrollV;
         if (follow)
            currPid = ProcessList_get(pl, currPos)->pid;
         if (recalculate)
            ProcessList_scan(pl);
         if (refreshTimeout == 0) {
            ProcessList_sort(pl);
            refreshTimeout = 1;
         }
         ListBox_prune(lb);
         int size = ProcessList_size(pl);
         int lbi = 0;
         for (int i = 0; i < size; i++) {
            Process* p = ProcessList_get(pl, i);
            if (!userOnly || (p->st_uid == userId)) {
               ListBox_set(lb, lbi, (Object*)p);
               if ((!follow && lbi == currPos) || (follow && p->pid == currPid)) {
                  ListBox_setSelected(lb, lbi);
                  lb->scrollV = currScrollV;
               }
               lbi++;
            }
         }
      }
      doRefresh = true;
      
      Header_draw(header);

      ListBox_draw(lb, true);
      int prev = ch;
      ch = getch();

      if (ch == ERR) {
         if (!incSearchMode)
            refreshTimeout--;
         if (prev == ch && !recalculate) {
            closeTimeout++;
            if (closeTimeout == 10)
               break;
         } else
            closeTimeout = 0;
         continue;
      }

      if (incSearchMode) {
         doRefresh = false;
         if (ch == KEY_F(3)) {
            int here = ListBox_getSelectedIndex(lb);
            int size = ProcessList_size(pl);
            int i = here+1;
            while (i != here) {
               if (i == size)
                  i = 0;
               Process* p = ProcessList_get(pl, i);
               if (String_contains_i(p->comm, incSearchBuffer)) {
                  ListBox_setSelected(lb, i);
                  break;
               }
               i++;
            }
            continue;
         } else if (isprint((char)ch) && (incSearchIndex < INCSEARCH_MAX)) {
            incSearchBuffer[incSearchIndex] = ch;
            incSearchIndex++;
            incSearchBuffer[incSearchIndex] = 0;
         } else if ((ch == KEY_BACKSPACE || ch == 127) && (incSearchIndex > 0)) {
            incSearchIndex--;
            incSearchBuffer[incSearchIndex] = 0;
         } else {
            incSearchMode = false;
            incSearchIndex = 0;
            incSearchBuffer[0] = 0;
            FunctionBar_draw(defaultBar, NULL);
            continue;
         }

         bool found = false;
         for (int i = 0; i < ProcessList_size(pl); i++) {
            Process* p = ProcessList_get(pl, i);
            if (String_contains_i(p->comm, incSearchBuffer)) {
               ListBox_setSelected(lb, i);
               found = true;
               break;
            }
         }
         if (found)
            FunctionBar_draw(searchBar, incSearchBuffer);
         else
            FunctionBar_drawAttr(searchBar, incSearchBuffer, CRT_colors[FAILED_SEARCH]);

         continue;
      }
      if (isdigit((char)ch)) {
         int pid = ch-48 + acc;
         for (int i = 0; i < ProcessList_size(pl) && ((Process*) ListBox_getSelected(lb))->pid != pid; i++)
            ListBox_setSelected(lb, i);
         acc = pid * 10;
         if (acc > 100000)
            acc = 0;
         continue;
      } else {
         acc = 0;
      }

      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK) {
            if (mevent.y >= lb->y + 1 && mevent.y < LINES - 1) {
               ListBox_setSelected(lb, mevent.y - lb->y + lb->scrollV - 1);
               doRefresh = false;
               refreshTimeout = resetRefreshTimeout;
               follow = true;
               continue;
            } if (mevent.y == LINES - 1) {
               FunctionBar* bar;
               if (incSearchMode) bar = searchBar;
               else bar = defaultBar;
               ch = FunctionBar_synthesizeEvent(bar, mevent.x);
            }

         }
      }

      switch (ch) {
      case KEY_RESIZE:
         ListBox_resize(lb, COLS, LINES-headerHeight-1);
         if (incSearchMode)
            FunctionBar_draw(searchBar, incSearchBuffer);
         else
            FunctionBar_draw(defaultBar, NULL);
         break;
      case 'M':
      {
         refreshTimeout = 0;
         pl->sortKey = PERCENT_MEM;
         pl->treeView = false;
         settings->changed = true;
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         break;
      }
      case 'T':
      {
         refreshTimeout = 0;
         pl->sortKey = TIME;
         pl->treeView = false;
         settings->changed = true;
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         break;
      }
      case 'U':
      {
         for (int i = 0; i < ListBox_getSize(lb); i++) {
            Process* p = (Process*) ListBox_get(lb, i);
            p->tag = false;
         }
         doRefresh = true;
         break;
      }
      case 'P':
      {
         refreshTimeout = 0;
         pl->sortKey = PERCENT_CPU;
         pl->treeView = false;
         settings->changed = true;
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         break;
      }
      case KEY_F(1):
      case 'h':
      {
         showHelp();
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
         Process* p = (Process*) ListBox_getSelected(lb);
         Process_toggleTag(p);
         ListBox_onKey(lb, KEY_DOWN);
         break;
      }
      case 's':
      {
         TraceScreen* ts = TraceScreen_new((Process*) ListBox_getSelected(lb));
         TraceScreen_run(ts);
         TraceScreen_delete(ts);
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
         Setup_run(settings, headerHeight);
         // TODO: shouldn't need this, colors should be dynamic
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         headerHeight = Header_calculateHeight(header);
         ListBox_move(lb, 0, headerHeight);
         ListBox_resize(lb, COLS, LINES-headerHeight-1);
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
         ListBox* lbu = ListBox_new(0, 0, 0, 0, LISTITEM_CLASS, true);
         ListBox_setHeader(lbu, "Show processes of:");
         UsersTable_foreach(ut, addUserToList, lbu);
         TypedVector_sort(lbu->items);
         ListItem* allUsers = ListItem_new("All users", -1);
         ListBox_insert(lbu, 0, (Object*) allUsers);
         char* fuFunctions[2] = {"Show    ", "Cancel "};
         ListItem* picked = (ListItem*) pickFromList(lb, lbu, 20, headerHeight, fuFunctions, defaultBar);
         if (picked) {
            if (picked == allUsers) {
               userOnly = false;
               break;
            } else {
               setUserOnly(ListItem_getRef(picked), &userOnly, &userId);
            }
         }
         break;
      }
      case KEY_F(9):
      case 'k':
      {
         if (!lbk) {
            lbk = (ListBox*) SignalsListBox_new(0, 0, 0, 0);
         }
         SignalsListBox_reset((SignalsListBox*) lbk);
         char* fuFunctions[2] = {"Send  ", "Cancel "};
         Signal* signal = (Signal*) pickFromList(lb, lbk, 15, headerHeight, fuFunctions, defaultBar);
         if (signal) {
            if (signal->number != 0) {
               ListBox_setHeader(lb, "Sending...");
               ListBox_draw(lb, true);
               refresh();
               bool anyTagged = false;
               for (int i = 0; i < ListBox_getSize(lb); i++) {
                  Process* p = (Process*) ListBox_get(lb, i);
                  if (p->tag) {
                     Process_sendSignal(p, signal->number);
                     Process_toggleTag(p);
                     anyTagged = true;
                  }
               }
               if (!anyTagged) {
                  Process* p = (Process*) ListBox_getSelected(lb);
                  Process_sendSignal(p, signal->number);
               }
               napms(500);
            }
         }
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         refreshTimeout = 0;
         break;
      }
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
         ListBox* lbf = ListBox_new(0,0,0,0,LISTITEM_CLASS,true);
         ListBox_setHeader(lbf, "Sort by");
         char* fuFunctions[2] = {"Sort  ", "Cancel "};
         ProcessField* fields = pl->fields;
         for (int i = 0; fields[i]; i++) {
            char* name = String_trim(Process_printField(fields[i]));
            ListBox_add(lbf, (Object*) ListItem_new(name, fields[i]));
            if (fields[i] == pl->sortKey)
               ListBox_setSelected(lbf, i);
            free(name);
         }
         ListItem* field = (ListItem*) pickFromList(lb, lbf, 15, headerHeight, fuFunctions, defaultBar);
         if (field) {
            pl->treeView = false;
            settings->changed = true;
            pl->sortKey = field->key;
         }
         ((Object*)lbf)->delete((Object*)lbf);
         ListBox_setRichHeader(lb, ProcessList_printHeader(pl));
         refreshTimeout = 0;
         break;
      }
      case 'I':
      case KEY_F(4):
      {
         refreshTimeout = 0;
         settings->changed = true;
         ProcessList_invertSortOrder(pl);
         break;
      }
      case KEY_F(8):
      case '[':
      case '=':
      case '+':
      {
         doRefresh = changePriority(lb, 1);
         break;
      }
      case KEY_F(7):
      case ']':
      case '-':
      {
         doRefresh = changePriority(lb, -1);
         break;
      }
      case KEY_F(3):
      case '/':
         FunctionBar_draw(searchBar, incSearchBuffer);
         incSearchMode = true;
         break;
      case 't':
      case KEY_F(5):
         refreshTimeout = 0;
         pl->treeView = !pl->treeView;
         settings->changed = true;
         break;
      case 'H':
         refreshTimeout = 0;
         pl->hideThreads = !pl->hideThreads;
         settings->changed = true;
         break;
      case 'K':
         refreshTimeout = 0;
         pl->hideKernelThreads = !pl->hideKernelThreads;
         settings->changed = true;
         break;
      default:
         doRefresh = false;
         refreshTimeout = resetRefreshTimeout;
         ListBox_onKey(lb, ch);
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
   FunctionBar_delete((Object*)searchBar);
   FunctionBar_delete((Object*)defaultBar);
   ((Object*)lb)->delete((Object*)lb);
   if (lbk)
      ((Object*)lbk)->delete((Object*)lbk);
   UsersTable_delete(ut);
   Settings_delete(settings);
   debug_done();
   return 0;
}
