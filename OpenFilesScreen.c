/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "OpenFilesScreen.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "Macros.h"
#include "Panel.h"
#include "ProvideCurses.h"
#include "Vector.h"
#include "XUtils.h"


// cf. getIndexForType; must be larger than the maximum value returned.
#define LSOF_DATACOL_COUNT 8

typedef struct OpenFiles_Data_ {
   char* data[LSOF_DATACOL_COUNT];
} OpenFiles_Data;

typedef struct OpenFiles_ProcessData_ {
   OpenFiles_Data data;
   int error;
   int cols[LSOF_DATACOL_COUNT];
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
      case 'o':
         return 7;
   }

   /* should never reach here */
   abort();
}

static const char* getDataForType(const OpenFiles_Data* data, char type) {
   size_t index = getIndexForType(type);
   return data->data[index] ? data->data[index] : "";
}

OpenFilesScreen* OpenFilesScreen_new(const Process* process) {
   OpenFilesScreen* this = xCalloc(1, sizeof(OpenFilesScreen));
   Object_setClass(this, Class(OpenFilesScreen));
   if (Process_isThread(process)) {
      this->pid = Process_getThreadGroup(process);
   } else {
      this->pid = Process_getPid(process);
   }
   return (OpenFilesScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 2, "   FD TYPE    MODE DEVICE           SIZE     OFFSET       NODE  NAME");
}

void OpenFilesScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

static void OpenFilesScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Snapshot of files open in process %d - %s", ((OpenFilesScreen*)this)->pid, Process_getCommand(this->process));
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   OpenFiles_ProcessData* pdata = xCalloc(1, sizeof(OpenFiles_ProcessData));
   pdata->cols[getIndexForType('s')] = 8;
   pdata->cols[getIndexForType('o')] = 8;
   pdata->cols[getIndexForType('i')] = 8;

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
      if (fdnull < 0) {
         exit(1);
      }

      dup2(fdnull, STDERR_FILENO);
      close(fdnull);
      char buffer[32] = {0};
      xSnprintf(buffer, sizeof(buffer), "%d", pid);
      // Use of NULL in variadic functions must have a pointer cast.
      // The NULL constant is not required by standard to have a pointer type.
      execlp("lsof", "lsof", "-P", "-o", "-p", buffer, "-F", (char*)NULL);
      exit(127);
   }
   close(fdpair[1]);

   OpenFiles_Data* item = &(pdata->data);
   OpenFiles_FileData* fdata = NULL;
   bool lsofIncludesFileSize = false;

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
            free_and_xStrdup(&item->data[index], line + 1);
            size_t dlen = strlen(item->data[index]);
            if (dlen > (size_t)pdata->cols[index]) {
               pdata->cols[index] = (int)CLAMP(dlen, 0, INT16_MAX);
            }
            break;
         }
         case 'o':  /* file's offset */
         {
            size_t index = getIndexForType(cmd);
            if (String_startsWith(line + 1, "0t")) {
               free_and_xStrdup(&item->data[index], line + 3);
            } else {
               free_and_xStrdup(&item->data[index], line + 1);
            }
            size_t dlen = strlen(item->data[index]);
            if (dlen > (size_t)pdata->cols[index]) {
               pdata->cols[index] = (int)CLAMP(dlen, 0, INT16_MAX);
            }
            break;
         }
         case 'c':  /* process command name  */
         case 'd':  /* file's device character code */
         case 'g':  /* process group ID */
         case 'G':  /* file flags */
         case 'k':  /* link count */
         case 'l':  /* file's lock status */
         case 'L':  /* process login name */
         case 'p':  /* process ID */
         case 'P':  /* protocol name */
         case 'R':  /* parent process ID */
         case 'T':  /* TCP/TPI information, identified by prefixes */
         case 'u':  /* process user ID */
            /* ignore */
            break;
      }

      if (cmd == 's')
         lsofIncludesFileSize = true;

      free(line);
   }
   fclose(fd);

   int wstatus;
   while (waitpid(child, &wstatus, 0) == -1)
      if (errno != EINTR) {
         pdata->error = 1;
         return pdata;
      }

   if (!WIFEXITED(wstatus)) {
      pdata->error = 1;
   } else {
      pdata->error = WEXITSTATUS(wstatus);
   }

   /* We got all information we need; no post-processing needed */
   if (lsofIncludesFileSize)
      return pdata;

   /* On linux, `lsof -o -F` omits SIZE, so add it back. */
   /* On macOS, `lsof -o -F` includes SIZE, so this block isn't needed.  If no open files have a filesize, this will still run, unfortunately. */
   size_t fileSizeIndex = getIndexForType('s');
   for (fdata = pdata->files; fdata != NULL; fdata = fdata->next) {
      item = &fdata->data;
      const char* filename = getDataForType(item, 'n');

      struct stat st;
      if (stat(filename, &st) == 0) {
         char fileSizeBuf[21]; /* 20 (long long) + 1 (NULL) */
         xSnprintf(fileSizeBuf, sizeof(fileSizeBuf), "%"PRIu64, (uint64_t)st.st_size); /* st.st_size is long long on macOS, long on linux */
         free_and_xStrdup(&item->data[fileSizeIndex], fileSizeBuf);
      }
   }

   return pdata;
}

static void OpenFiles_Data_clear(OpenFiles_Data* data) {
   for (size_t i = 0; i < ARRAYSIZE(data->data); i++)
      free(data->data[i]);
}

static void OpenFilesScreen_scan(InfoScreen* super) {
   Panel* panel = super->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   OpenFiles_ProcessData* pdata = OpenFilesScreen_getProcessData(((OpenFilesScreen*)super)->pid);
   if (pdata->error == 127) {
      InfoScreen_addLine(super, "Could not execute 'lsof'. Please make sure it is available in your $PATH.");
   } else if (pdata->error == 1) {
      InfoScreen_addLine(super, "Failed listing open files.");
   } else {
      char hdrbuf[128] = {0};
      snprintf(hdrbuf, sizeof(hdrbuf), "%5.5s %-7.7s %-4.4s %6.6s %*s %*s %*s  %s",
         "FD", "TYPE", "MODE", "DEVICE",
         pdata->cols[getIndexForType('s')], "SIZE",
         pdata->cols[getIndexForType('o')], "OFFSET",
         pdata->cols[getIndexForType('i')], "NODE",
         "NAME"
      );
      Panel_setHeader(panel, hdrbuf);

      OpenFiles_FileData* fdata = pdata->files;
      while (fdata) {
         OpenFiles_Data* data = &fdata->data;
         char* entry = NULL;
         xAsprintf(&entry, "%5.5s %-7.7s %-4.4s %6.6s %*s %*s %*s  %s",
                   getDataForType(data, 'f'),
                   getDataForType(data, 't'),
                   getDataForType(data, 'a'),
                   getDataForType(data, 'D'),
                   pdata->cols[getIndexForType('s')],
                   getDataForType(data, 's'),
                   pdata->cols[getIndexForType('o')],
                   getDataForType(data, 'o'),
                   pdata->cols[getIndexForType('i')],
                   getDataForType(data, 'i'),
                   getDataForType(data, 'n'));
         InfoScreen_addLine(super, entry);
         free(entry);
         OpenFiles_Data_clear(data);
         OpenFiles_FileData* old = fdata;
         fdata = fdata->next;
         free(old);
      }
      OpenFiles_Data_clear(&pdata->data);
   }
   free(pdata);
   Vector_insertionSort(super->lines);
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
