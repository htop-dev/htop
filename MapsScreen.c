/*
htop - MapsScreen.c
(C) 2025 Pierre Wilke
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "MapsScreen.h"

#include <stdlib.h>
#include <string.h>

#include "Macros.h"
#include "Panel.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "Vector.h"
#include "XUtils.h"


MapsScreen* MapsScreen_new(Process* process) {
   MapsScreen* this = xMalloc(sizeof(MapsScreen));
   Object_setClass(this, Class(MapsScreen));
   return (MapsScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 2, " ");
}

void MapsScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

static void MapsScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Maps of process %d - %s", Process_getPid(this->process), Process_getCommand(this->process));
}

static void MapsScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   char* maps = Platform_getProcessMaps(Process_getPid(this->process));
   if (maps) {
      char* p = maps;
      char* pp = maps;
      while (*p) {
         if (*p == '\n') {
            *p = '\0';
            InfoScreen_addLine(this, pp);
            pp = p + 1;
         }
         p++;
      }
      free(maps);
   } else {
      InfoScreen_addLine(this, "Could not read process maps.");
   }

   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}

const InfoScreenClass MapsScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = MapsScreen_delete
   },
   .scan = MapsScreen_scan,
   .draw = MapsScreen_draw
};
