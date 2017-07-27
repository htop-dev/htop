/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "OpenFilesScreen.h"

#include "CRT.h"
#include "ProcessList.h"
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
#include "InfoScreen.h"

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
   InfoScreen super;
   pid_t pid;
} OpenFilesScreen;

}*/

InfoScreenClass OpenFilesScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = OpenFilesScreen_delete
   },
   .scan = OpenFilesScreen_scan,
   .draw = OpenFilesScreen_draw
};

OpenFilesScreen* OpenFilesScreen_new(Process* process) {
   OpenFilesScreen* this = xMalloc(sizeof(OpenFilesScreen));
   Object_setClass(this, Class(OpenFilesScreen));
   if (Process_isThread(process))
      this->pid = process->tgid;
   else
      this->pid = process->pid;
   return (OpenFilesScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, "   FD TYPE     DEVICE       SIZE       NODE NAME");
}

void OpenFilesScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void OpenFilesScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Snapshot of files open in process %d - %s", ((OpenFilesScreen*)this)->pid, this->process->comm);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   char command[1025];
   xSnprintf(command, 1024, "lsof -P -p %d -F 2> /dev/null", pid);
   FILE* fd = popen(command, "r");
   OpenFiles_ProcessData* pdata = xCalloc(1, sizeof(OpenFiles_ProcessData));
   OpenFiles_FileData* fdata = NULL;
   OpenFiles_Data* item = &(pdata->data);
   if (!fd) {
      pdata->error = 127;
      return pdata;
   }
   for (;;) {
      char* line = String_readLine(fd);
      if (!line) {
         break;
      }
      unsigned char cmd = line[0];
      if (cmd == 'f') {
         OpenFiles_FileData* nextFile = xCalloc(1, sizeof(OpenFiles_FileData));
         if (fdata == NULL) {
            pdata->files = nextFile;
         } else {
            fdata->next = nextFile;
         }
         fdata = nextFile;
         item = &(fdata->data);
      }
      item->data[cmd] = xStrdup(line + 1);
      free(line);
   }
   pdata->error = pclose(fd);
   return pdata;
}

static inline void OpenFiles_Data_clear(OpenFiles_Data* data) {
   for (int i = 0; i < 255; i++)
      if (data->data[i])
         free(data->data[i]);
}

void OpenFilesScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   OpenFiles_ProcessData* pdata = OpenFilesScreen_getProcessData(((OpenFilesScreen*)this)->pid);
   if (pdata->error == 127) {
      InfoScreen_addLine(this, "Could not execute 'lsof'. Please make sure it is available in your $PATH.");
   } else if (pdata->error == 1) {
      InfoScreen_addLine(this, "Failed listing open files.");
   } else {
      OpenFiles_FileData* fdata = pdata->files;
      while (fdata) {
         char** data = fdata->data.data;
         int lenN = data['n'] ? strlen(data['n']) : 0;
         int sizeEntry = 5 + 7 + 10 + 10 + 10 + lenN + 5 /*spaces*/ + 1 /*null*/;
         char* entry = xMalloc(sizeEntry);
         xSnprintf(entry, sizeEntry, "%5.5s %7.7s %10.10s %10.10s %10.10s %s",
            data['f'] ? data['f'] : "",
            data['t'] ? data['t'] : "",
            data['D'] ? data['D'] : "",
            data['s'] ? data['s'] : "",
            data['i'] ? data['i'] : "",
            data['n'] ? data['n'] : "");
         InfoScreen_addLine(this, entry);
         OpenFiles_Data_clear(&fdata->data);
         OpenFiles_FileData* old = fdata;
         fdata = fdata->next;
         free(old);
      }
      OpenFiles_Data_clear(&pdata->data);
   }
   free(pdata);
   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}
