/*
htop - linux/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "IOPriority.h"
#include "IOPriorityPanel.h"
#include "LinuxProcess.h"

/*{
#include "Action.h"
}*/

static Htop_Reaction Platform_actionSetIOPriority(Panel* panel, ProcessList* pl, Header* header) {
   (void) panel, (void) pl;
   LinuxProcess* p = (LinuxProcess*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   IOPriority ioprio = p->ioPriority;
   Panel* ioprioPanel = IOPriorityPanel_new(ioprio);
   const char* fuFunctions[] = {"Set    ", "Cancel ", NULL};
   void* set = Action_pickFromVector(panel, ioprioPanel, 21, fuFunctions, header);
   if (set) {
      IOPriority ioprio = IOPriorityPanel_getIOPriority(ioprioPanel);
      bool ok = Action_foreachProcess(panel, (Action_ForeachProcessFn) LinuxProcess_setIOPriority, (size_t) ioprio, NULL);
      if (!ok)
         beep();
   }
   Panel_delete((Object*)ioprioPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

void Platform_setBindings(Htop_Action* keys) {
   keys['i'] = Platform_actionSetIOPriority;
}
