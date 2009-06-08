/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "OpenFilesScreen.h"
#include "ProcessList.h"
#include "Process.h"
#include "ListItem.h"
#include "Panel.h"
#include "FunctionBar.h"

/*{

typedef struct OpenFiles_ProcessData_ {
   char* data[256];
   struct OpenFiles_FileData_* files;
   bool failed;
} OpenFiles_ProcessData;

typedef struct OpenFiles_FileData_ {
   char* data[256];
   struct OpenFiles_FileData_* next;
} OpenFiles_FileData;

typedef struct OpenFilesScreen_ {
   Process* process;
   Panel* display;
   FunctionBar* bar;
   bool tracing;
} OpenFilesScreen;

}*/

static char* tbFunctions[] = {"Refresh", "Done   ", NULL};

static char* tbKeys[] = {"F5", "Esc"};

static int tbEvents[] = {KEY_F(5), 27};

OpenFilesScreen* OpenFilesScreen_new(Process* process) {
   OpenFilesScreen* this = (OpenFilesScreen*) malloc(sizeof(OpenFilesScreen));
   this->process = process;
   this->display = Panel_new(0, 1, COLS, LINES-3, LISTITEM_CLASS, true, ListItem_compare);
   this->bar = FunctionBar_new(tbFunctions, tbKeys, tbEvents);
   this->tracing = true;
   return this;
}

void OpenFilesScreen_delete(OpenFilesScreen* this) {
   Panel_delete((Object*)this->display);
   FunctionBar_delete((Object*)this->bar);
   free(this);
}

static void OpenFilesScreen_draw(OpenFilesScreen* this) {
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Files open in process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true);
   FunctionBar_draw(this->bar, NULL);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(int pid) {
   char command[1025];
   snprintf(command, 1024, "lsof -p %d -F 2> /dev/null", pid);
   FILE* fd = popen(command, "r");
   OpenFiles_ProcessData* process = calloc(sizeof(OpenFiles_ProcessData), 1);
   OpenFiles_FileData* file = NULL;
   OpenFiles_ProcessData* item = process;
   process->failed = true;
   bool anyRead = false;
   while (!feof(fd)) {
      int cmd = fgetc(fd);
      if (cmd == EOF && !anyRead) {
         process->failed = true;
         break;
      }
      anyRead = true;
      process->failed = false;
      char* entry = malloc(1024);
      if (!fgets(entry, 1024, fd)) break;
      char* newline = strrchr(entry, '\n');
      *newline = '\0';
      if (cmd == 'f') {
         OpenFiles_FileData* nextFile = calloc(sizeof(OpenFiles_ProcessData), 1);
         if (file == NULL) {
            process->files = nextFile;
         } else {
            file->next = nextFile;
         }
         file = nextFile;
         item = (OpenFiles_ProcessData*) file;
      }
      item->data[cmd] = entry;
   }
   pclose(fd);
   return process;
}

static void OpenFilesScreen_scan(OpenFilesScreen* this) {
   Panel* panel = this->display;
   int index = MAX(Panel_getSelectedIndex(panel), 0);
   Panel_prune(panel);
   OpenFiles_ProcessData* process = OpenFilesScreen_getProcessData(this->process->pid);
   if (process->failed) {
      Panel_add(panel, (Object*) ListItem_new("Could not execute 'lsof'. Please make sure it is available in your $PATH.", 0));
   } else {
      OpenFiles_FileData* file = process->files;
      while (file) {
         char entry[1024];
         sprintf(entry, "%5s %4s %10s %10s %10s %s",
            file->data['f'] ? file->data['f'] : "",
            file->data['t'] ? file->data['t'] : "",
            file->data['D'] ? file->data['D'] : "",
            file->data['s'] ? file->data['s'] : "",
            file->data['i'] ? file->data['i'] : "",
            file->data['n'] ? file->data['n'] : "");
         Panel_add(panel, (Object*) ListItem_new(entry, 0));
         for (int i = 0; i < 255; i++)
            if (file->data[i])
               free(file->data[i]);
         OpenFiles_FileData* old = file;
         file = file->next;
         free(old);
      }
      for (int i = 0; i < 255; i++)
         if (process->data[i])
            free(process->data[i]);
   }
   free(process);
   Vector_sort(panel->items);
   Panel_setSelected(panel, index);
}

void OpenFilesScreen_run(OpenFilesScreen* this) {
   Panel* panel = this->display;
   Panel_setHeader(panel, "   FD TYPE     DEVICE       SIZE       NODE NAME");
   OpenFilesScreen_scan(this);
   OpenFilesScreen_draw(this);
   //CRT_disableDelay();
   
   bool looping = true;
   while (looping) {
      Panel_draw(panel, true);
      int ch = getch();
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = FunctionBar_synthesizeEvent(this->bar, mevent.x);
      }
      switch(ch) {
      case ERR:
         continue;
      case KEY_F(5):
         clear();
         OpenFilesScreen_scan(this);
         OpenFilesScreen_draw(this);
         break;
      case '\014': // Ctrl+L
         clear();
         OpenFilesScreen_draw(this);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         OpenFilesScreen_draw(this);
         break;
      default:
         Panel_onKey(panel, ch);
      }
   }
   //CRT_enableDelay();
}
