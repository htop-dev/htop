/*
htop - ProcessLocksScreen.c
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProcessLocksScreen.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include "Panel.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "Vector.h"
#include "XUtils.h"


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

static inline void FileLocks_Data_clear(FileLocks_Data* data) {
   free(data->locktype);
   free(data->exclusive);
   free(data->readwrite);
   free(data->filename);
}

static void ProcessLocksScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   FileLocks_ProcessData* pdata = Platform_getProcessLocks(((ProcessLocksScreen*)this)->pid);
   if (!pdata) {
      InfoScreen_addLine(this, "This feature is not supported on your platform.");
   } else if (pdata->error) {
      InfoScreen_addLine(this, "Could not determine file locks.");
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
               data->locktype, data->exclusive, data->readwrite,
               data->dev[0], data->dev[1], data->inode,
               data->start, "<END OF FILE>",
               data->filename ? data->filename : "<N/A>"
            );
         } else {
            xSnprintf(entry, sizeof(entry), "%10d  %-10s %-10s %-10s %02x:%02x:%020"PRIu64" %20"PRIu64" %20"PRIu64"  %s",
               data->id,
               data->locktype, data->exclusive, data->readwrite,
               data->dev[0], data->dev[1], data->inode,
               data->start, data->end,
               data->filename ? data->filename : "<N/A>"
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
