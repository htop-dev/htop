/*
htop - ProcessLocksScreen.c
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProcessLocksScreen.h"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "IncSet.h"
#include "ProcessList.h"
#include "XUtils.h"


typedef struct FileLocks_Data_ {
   char* data[4];
   int id;
   unsigned int dev[2];
   uint64_t inode;
   uint64_t start;
   uint64_t end;
} FileLocks_Data;

typedef struct FileLocks_ProcessData_ {
   bool error;
   struct FileLocks_LockData_* locks;
} FileLocks_ProcessData;

typedef struct FileLocks_LockData_ {
   FileLocks_Data data;
   struct FileLocks_LockData_* next;
} FileLocks_LockData;

ProcessLocksScreen* ProcessLocksScreen_new(const Process* process) {
   ProcessLocksScreen* this = xMalloc(sizeof(ProcessLocksScreen));
   Object_setClass(this, Class(ProcessLocksScreen));
   if (Process_isThread(process))
      this->pid = process->tgid;
   else
      this->pid = process->pid;
   return (ProcessLocksScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, "        ID  TYPE       EXCLUSION  READ/WRITE DEVICE:INODE                              START                  END  FILENAME");
}

void ProcessLocksScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

static void ProcessLocksScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Snapshot of file locks of process %d - %s", ((ProcessLocksScreen*)this)->pid, this->process->comm);
}

/*
 * Return the absolute path of a file given its pid&inode number
 *
 * Based on implementation of lslocks from util-linux:
 * https://sources.debian.org/src/util-linux/2.36-3/misc-utils/lslocks.c/#L162
 */
static char *ProcessLocksScreen_getInodeFilename(pid_t pid, ino_t inode) {
   struct stat sb;
   struct dirent *de;
   DIR *dirp;
   size_t len;
   int fd;

   char path[PATH_MAX];
   char sym[PATH_MAX];
   char* ret = NULL;

   memset(path, 0, sizeof(path));
   memset(sym, 0, sizeof(sym));

   xSnprintf(path, sizeof(path), "%s/%d/fd/", PROCDIR, pid);
   if (strlen(path) >= (sizeof(path) - 2))
      return NULL;

   if (!(dirp = opendir(path)))
      return NULL;

   if ((fd = dirfd(dirp)) < 0 )
      goto out;

   while ((de = readdir(dirp))) {
      if (String_eq(de->d_name, ".") || String_eq(de->d_name, ".."))
         continue;

      /* care only for numerical descriptors */
      if (!strtoull(de->d_name, (char **) NULL, 10))
         continue;

#if !defined(HAVE_FSTATAT) || !defined(HAVE_READLINKAT)
      char filepath[PATH_MAX + 1];
      xSnprintf(filepath, sizeof(filepath), "%s/%s", path, de->d_name);
#endif

#ifdef HAVE_FSTATAT
      if (!fstatat(fd, de->d_name, &sb, 0) && inode != sb.st_ino)
         continue;
#else
      if (!stat(filepath, &sb)) && inode != sb.st_ino)
         continue;
#endif

#ifdef HAVE_READLINKAT
      if ((len = readlinkat(fd, de->d_name, sym, sizeof(sym) - 1)) < 1)
         goto out;
#else
      if ((len = readlink(filepath, sym, sizeof(sym) - 1)) < 1)
         goto out;
#endif

      sym[len] = '\0';

      ret = xStrdup(sym);
      break;
   }

out:
   closedir(dirp);
   return ret;
}

static FileLocks_ProcessData* ProcessLocksScreen_getProcessData(pid_t pid) {
   FileLocks_ProcessData* pdata = xCalloc(1, sizeof(FileLocks_ProcessData));

   FILE* f = fopen(PROCDIR "/locks", "r");
   if (!f) {
      pdata->error = true;
      return pdata;
   }

   char buffer[1024];
   FileLocks_LockData** data_ref = &pdata->locks;
   while(fgets(buffer, sizeof(buffer), f)) {
      if (!strchr(buffer, '\n'))
         continue;

      int lock_id;
      char lock_type[16];
      char lock_excl[16];
      char lock_rw[16];
      pid_t lock_pid;
      unsigned int lock_dev[2];
      uint64_t lock_inode;
      char lock_start[25];
      char lock_end[25];

      if (10 != sscanf(buffer, "%d:  %15s  %15s %15s %d %x:%x:%"PRIu64" %24s %24s",
         &lock_id, lock_type, lock_excl, lock_rw, &lock_pid,
         &lock_dev[0], &lock_dev[1], &lock_inode,
         lock_start, lock_end))
         continue;

      if (pid != lock_pid)
         continue;

      FileLocks_LockData* ldata = xCalloc(1, sizeof(FileLocks_LockData));
      FileLocks_Data* data = &ldata->data;
      data->id = lock_id;
      data->data[0] = xStrdup(lock_type);
      data->data[1] = xStrdup(lock_excl);
      data->data[2] = xStrdup(lock_rw);
      data->data[3] = ProcessLocksScreen_getInodeFilename(lock_pid, lock_inode);
      data->dev[0] = lock_dev[0];
      data->dev[1] = lock_dev[1];
      data->inode = lock_inode;
      data->start = strtoull(lock_start, NULL, 10);
      if (!String_eq(lock_end, "EOF")) {
         data->end = strtoull(lock_end, NULL, 10);
      } else {
         data->end = ULLONG_MAX;
      }

      *data_ref = ldata;
      data_ref = &ldata->next;
   }

   fclose(f);
   return pdata;
}

static inline void FileLocks_Data_clear(FileLocks_Data* data) {
   for (size_t i = 0; i < ARRAYSIZE(data->data); i++)
      free(data->data[i]);
}

static void ProcessLocksScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   FileLocks_ProcessData* pdata = ProcessLocksScreen_getProcessData(((ProcessLocksScreen*)this)->pid);
   if (pdata->error) {
      InfoScreen_addLine(this, "Could not read file locks.");
   } else {
      FileLocks_LockData* ldata = pdata->locks;
      if (!ldata) {
         InfoScreen_addLine(this, "No locks have been found for the selected process.");
      }
      while (ldata) {
         FileLocks_Data* data = &ldata->data;

         char entry[512];
         if (ULLONG_MAX == data->end) {
            xSnprintf(entry, sizeof(entry), "%10d  %-10s %-10s %-10s %02x:%02x:%020"PRIu64" %20"PRIu64" %20s  %s",
               data->id,
               data->data[0], data->data[1], data->data[2],
               data->dev[0], data->dev[1], data->inode,
               data->start, "<END OF FILE>",
               data->data[3] ? data->data[3] : "<N/A>"
            );
         } else {
            xSnprintf(entry, sizeof(entry), "%10d  %-10s %-10s %-10s %02x:%02x:%020"PRIu64" %20"PRIu64" %20"PRIu64"  %s",
               data->id,
               data->data[0], data->data[1], data->data[2],
               data->dev[0], data->dev[1], data->inode,
               data->start, data->end,
               data->data[3] ? data->data[3] : "<N/A>"
            );
         }

         InfoScreen_addLine(this, entry);
         FileLocks_Data_clear(&ldata->data);

         FileLocks_LockData* old = ldata;
         ldata = ldata->next;
         free(old);
      }
   }
   free(pdata);
   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}

const InfoScreenClass ProcessLocksScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = ProcessLocksScreen_delete
   },
   .scan = ProcessLocksScreen_scan,
   .draw = ProcessLocksScreen_draw
};
