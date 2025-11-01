/*
htop - EnvScreen.c
(C) 2015,2016 Michael Klein
(C) 2016,2017 Hisham H. Muhammad
(C) 2020,2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "EnvScreen.h"

#include <stdlib.h>
#include <string.h>

#include "Macros.h"
#include "Panel.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "Vector.h"
#include "XUtils.h"


EnvScreen* EnvScreen_new(Process* process) {
   EnvScreen* this = xMalloc(sizeof(EnvScreen));
   Object_setClass(this, Class(EnvScreen));
   return (EnvScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 2, " ");
}

void EnvScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

static void EnvScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Environment of process %d - %s", Process_getPid(this->process), Process_getCommand(this->process));
}

static void EnvScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   char* env = Platform_getProcessEnv(Process_getPid(this->process));
   if (env) {
      for (const char* p = env; *p; p = strrchr(p, 0) + 1)
         InfoScreen_addLine(this, p);
      free(env);
   }
   else {
      InfoScreen_addLine(this, "Could not read process environment.");
   }

   Vector_sort(this->lines, NULL);
   Vector_sort(panel->items, NULL);
   Panel_setSelected(panel, idx);
}

const InfoScreenClass EnvScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = EnvScreen_delete
   },
   .scan = EnvScreen_scan,
   .draw = EnvScreen_draw
};
