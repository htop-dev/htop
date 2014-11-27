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
#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "BatteryMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"

#include <math.h>

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

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

int Platform_getUptime() {
   double uptime = 0;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   if (fd) {
      fscanf(fd, "%64lf", &uptime);
      fclose(fd);
   }
   int totalseconds = (int) floor(uptime);
}
