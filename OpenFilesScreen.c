/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "OpenFilesScreen.h"

#include "CRT.h"
#include "ProcessList.h"
#include "ListItem.h"
#include "IncSet.h"
#include "StringUtils.h"
#include "FunctionBar.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/*{
#include "Process.h"
#include "Panel.h"

typedef struct OpenFiles_Data_ {
   char* data[256];
} OpenFiles_Data;

typedef struct OpenFiles_ProcessData_ {
   OpenFiles_Data data;
   int error;
   struct OpenFiles_FileData_* files;
} OpenFiles_ProcessData;

typedef struct OpenFiles_FileData_ {
   OpenFiles_Data data;
   struct OpenFiles_FileData_* next;
} OpenFiles_FileData;

typedef struct OpenFilesScreen_ {
   Process* process;
   pid_t pid;
   Panel* display;
} OpenFilesScreen;

}*/

static const char* OpenFilesScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* OpenFilesScreenKeys[] = {"F3", "F4", "F5", "Esc"};

static int OpenFilesScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

OpenFilesScreen* OpenFilesScreen_new(Process* process) {
   OpenFilesScreen* this = (OpenFilesScreen*) malloc(sizeof(OpenFilesScreen));
   this->process = process;
   FunctionBar* bar = FunctionBar_new(OpenFilesScreenFunctions, OpenFilesScreenKeys, OpenFilesScreenEvents);
   this->display = Panel_new(0, 1, COLS, LINES-3, false, Class(ListItem), bar);
   if (Process_isThread(process))
      this->pid = process->tgid;
   else
      this->pid = process->pid;
   return this;
}

void OpenFilesScreen_delete(OpenFilesScreen* this) {
   Panel_delete((Object*)this->display);
   free(this);
}

static void OpenFilesScreen_draw(OpenFilesScreen* this, IncSet* inc) {
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Snapshot of files open in process %d - %s", this->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true);
   IncSet_drawBar(inc);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   char command[1025];
   snprintf(command, 1024, "lsof -P -p %d -F 2> /dev/null", pid);
   FILE* fd = popen(command, "r");
   OpenFiles_ProcessData* pdata = calloc(1, sizeof(OpenFiles_ProcessData));
   OpenFiles_FileData* fdata = NULL;
   OpenFiles_Data* item = &(pdata->data);
   if (!fd) {
      pdata->error = 127;
      return pdata;
   }
   while (!feof(fd)) {
      int cmd = fgetc(fd);
      if (cmd == EOF)
         break;
      char* entry = malloc(1024);
      if (!fgets(entry, 1024, fd)) {
         free(entry);
         break;
      }
      char* newline = strrchr(entry, '\n');
      *newline = '\0';
      if (cmd == 'f') {
         OpenFiles_FileData* nextFile = calloc(1, sizeof(OpenFiles_FileData));
         if (fdata == NULL) {
            pdata->files = nextFile;
         } else {
            fdata->next = nextFile;
         }
         fdata = nextFile;
         item = &(fdata->data);
      }
      assert(cmd >= 0 && cmd <= 0xff);
      item->data[cmd] = entry;
   }
   pdata->error = pclose(fd);
   return pdata;
}

static inline void addLine(const char* line, Vector* lines, Panel* panel, const char* incFilter) {
   Vector_add(lines, (Object*) ListItem_new(line, 0));
   if (!incFilter || String_contains_i(line, incFilter))
      Panel_add(panel, (Object*)Vector_get(lines, Vector_size(lines)-1));
}

static inline void OpenFiles_Data_clear(OpenFiles_Data* data) {
   for (int i = 0; i < 255; i++)
      if (data->data[i])
         free(data->data[i]);
}

static void OpenFilesScreen_scan(OpenFilesScreen* this, Vector* lines, IncSet* inc) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   OpenFiles_ProcessData* pdata = OpenFilesScreen_getProcessData(this->pid);
   if (pdata->error == 127) {
      addLine("Could not execute 'lsof'. Please make sure it is available in your $PATH.", lines, panel, IncSet_filter(inc));
   } else if (pdata->error == 1) {
      addLine("Failed listing open files.", lines, panel, IncSet_filter(inc));
   } else {
      OpenFiles_FileData* fdata = pdata->files;
      while (fdata) {
         char entry[1024];
         char** data = fdata->data.data;
         sprintf(entry, "%5s %4s %10s %10s %10s %s",
            data['f'] ? data['f'] : "",
            data['t'] ? data['t'] : "",
            data['D'] ? data['D'] : "",
            data['s'] ? data['s'] : "",
            data['i'] ? data['i'] : "",
            data['n'] ? data['n'] : "");
         addLine(entry, lines, panel, IncSet_filter(inc));
         OpenFiles_Data_clear(&fdata->data);
         OpenFiles_FileData* old = fdata;
         fdata = fdata->next;
         free(old);
      }
      OpenFiles_Data_clear(&pdata->data);
   }
   free(pdata);
   Vector_insertionSort(lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}

void OpenFilesScreen_run(OpenFilesScreen* this) {
   Panel* panel = this->display;
   Panel_setHeader(panel, "   FD TYPE     DEVICE       SIZE       NODE NAME");

   FunctionBar* bar = panel->defaultBar;
   IncSet* inc = IncSet_new(bar);
   
   Vector* lines = Vector_new(panel->items->type, true, DEFAULT_SIZE);

   OpenFilesScreen_scan(this, lines, inc);
   OpenFilesScreen_draw(this, inc);
   
   bool looping = true;
   while (looping) {
   
      Panel_draw(panel, true);
      
      if (inc->active)
         move(LINES-1, CRT_cursorX);
      int ch = getch();
      
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = IncSet_synthesizeEvent(inc, mevent.x);
      }

      if (inc->active) {
         IncSet_handleKey(inc, ch, panel, IncSet_getListItemValue, lines);
         continue;
      }

      switch(ch) {
      case ERR:
         continue;
      case KEY_F(3):
      case '/':
         IncSet_activate(inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(inc, INC_FILTER, panel);
         break;
      case KEY_F(5):
         clear();
         OpenFilesScreen_scan(this, lines, inc);
         OpenFilesScreen_draw(this, inc);
         break;
      case '\014': // Ctrl+L
         clear();
         OpenFilesScreen_draw(this, inc);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         OpenFilesScreen_draw(this, inc);
         break;
      default:
         Panel_onKey(panel, ch);
      }
   }

   Vector_delete(lines);
   IncSet_delete(inc);
}
