#include "EnvScreen.h"

#include "config.h"
#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "Platform.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


InfoScreenClass EnvScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = EnvScreen_delete
   },
   .scan = EnvScreen_scan,
   .draw = EnvScreen_draw
};

EnvScreen* EnvScreen_new(Process* process) {
   EnvScreen* this = xMalloc(sizeof(EnvScreen));
   Object_setClass(this, Class(EnvScreen));
   return (EnvScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void EnvScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void EnvScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Environment of process %d - %s", this->process->pid, this->process->comm);
}

void EnvScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   CRT_dropPrivileges();
   char* env = Platform_getProcessEnv(this->process->pid);
   CRT_restorePrivileges();
   if (env) {
      for (char *p = env; *p; p = strrchr(p, 0)+1)
         InfoScreen_addLine(this, p);
      free(env);
   }
   else {
      InfoScreen_addLine(this, "Could not read process environment.");
   }

   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}
