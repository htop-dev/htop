/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
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


typedef struct OpenFiles_Data_ {
   char* data[7];
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

static size_t getIndexForType(char type) {
   switch (type) {
   case 'f':
      return 0;
   case 'a':
      return 1;
   case 'D':
      return 2;
   case 'i':
      return 3;
   case 'n':
      return 4;
   case 's':
      return 5;
   case 't':
      return 6;
   }

   /* should never reach here */
   abort();
}

static const char* getDataForType(const OpenFiles_Data* data, char type) {
   size_t index = getIndexForType(type);
   return data->data[index] ? data->data[index] : "";
}

OpenFilesScreen* OpenFilesScreen_new(const Process* process) {
   OpenFilesScreen* this = xMalloc(sizeof(OpenFilesScreen));
   Object_setClass(this, Class(OpenFilesScreen));
   if (Process_isThread(process))
      this->pid = process->tgid;
   else
      this->pid = process->pid;
   return (OpenFilesScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, "   FD TYPE    MODE DEVICE           SIZE       NODE  NAME");
}

void OpenFilesScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

static void OpenFilesScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Snapshot of files open in process %d - %s", ((OpenFilesScreen*)this)->pid, this->process->comm);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   OpenFiles_ProcessData* pdata = xCalloc(1, sizeof(OpenFiles_ProcessData));

   int fdpair[2] = {0, 0};
   if (pipe(fdpair) == -1) {
      pdata->error = 1;
      return pdata;
   }

   pid_t child = fork();
   if (child == -1) {
      close(fdpair[1]);
      close(fdpair[0]);
      pdata->error = 1;
      return pdata;
   }

   if (child == 0) {
      close(fdpair[0]);
      dup2(fdpair[1], STDOUT_FILENO);
      close(fdpair[1]);
      int fdnull = open("/dev/null", O_WRONLY);
      if (fdnull < 0)
         exit(1);
      dup2(fdnull, STDERR_FILENO);
      close(fdnull);
      char buffer[32] = {0};
      xSnprintf(buffer, sizeof(buffer), "%d", pid);
      execlp("lsof", "lsof", "-P", "-p", buffer, "-F", NULL);
      exit(127);
   }
   close(fdpair[1]);

   OpenFiles_Data* item = &(pdata->data);
   OpenFiles_FileData* fdata = NULL;

   FILE* fd = fdopen(fdpair[0], "r");
   if (!fd) {
      pdata->error = 1;
      return pdata;
   }
   for (;;) {
      char* line = String_readLine(fd);
      if (!line) {
         break;
      }

      unsigned char cmd = line[0];
      switch (cmd) {
      case 'f':  /* file descriptor */
      {
         OpenFiles_FileData* nextFile = xCalloc(1, sizeof(OpenFiles_FileData));
         if (fdata == NULL) {
            pdata->files = nextFile;
         } else {
            fdata->next = nextFile;
         }
         fdata = nextFile;
         item = &(fdata->data);
      } /* FALLTHRU */
      case 'a':  /* file access mode */
      case 'D':  /* file's major/minor device number */
      case 'i':  /* file's inode number */
      case 'n':  /* file name, comment, Internet address */
      case 's':  /* file's size */
      case 't':  /* file's type */
      {
         size_t index = getIndexForType(cmd);
         free(item->data[index]);
         item->data[index] = xStrdup(line + 1);
         break;
      }
      case 'c':  /* process command name  */
      case 'd':  /* file's device character code */
      case 'g':  /* process group ID */
      case 'G':  /* file flags */
      case 'k':  /* link count */
      case 'l':  /* file's lock status */
      case 'L':  /* process login name */
      case 'o':  /* file's offset */
      case 'p':  /* process ID */
      case 'P':  /* protocol name */
      case 'R':  /* parent process ID */
      case 'T':  /* TCP/TPI information, identified by prefixes */
      case 'u':  /* process user ID */
         /* ignore */
         break;
      }
      free(line);
   }
   fclose(fd);

   int wstatus;
   if (waitpid(child, &wstatus, 0) == -1) {
      pdata->error = 1;
      return pdata;
   }

   if (!WIFEXITED(wstatus))
      pdata->error = 1;
   else
      pdata->error = WEXITSTATUS(wstatus);

   return pdata;
}

static void OpenFiles_Data_clear(OpenFiles_Data* data) {
   for (size_t i = 0; i < ARRAYSIZE(data->data); i++)
      free(data->data[i]);
}

static void OpenFilesScreen_scan(InfoScreen* this) {
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
         OpenFiles_Data* data = &fdata->data;
         size_t lenN = strlen(getDataForType(data, 'n'));
         size_t sizeEntry = 5 + 7 + 4 + 10 + 10 + 10 + lenN + 7 /*spaces*/ + 1 /*null*/;
         char entry[sizeEntry];
         xSnprintf(entry, sizeof(entry), "%5.5s %-7.7s %-4.4s %-10.10s %10.10s %10.10s  %s",
                   getDataForType(data, 'f'),
                   getDataForType(data, 't'),
                   getDataForType(data, 'a'),
                   getDataForType(data, 'D'),
                   getDataForType(data, 's'),
                   getDataForType(data, 'i'),
                   getDataForType(data, 'n'));
         InfoScreen_addLine(this, entry);
         OpenFiles_Data_clear(data);
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

const InfoScreenClass OpenFilesScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = OpenFilesScreen_delete
   },
   .scan = OpenFilesScreen_scan,
   .draw = OpenFilesScreen_draw
};
