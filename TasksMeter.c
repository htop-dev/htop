/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TasksMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include "CRT.h"

#include "debug.h"

/*{

typedef struct TasksMeter_ TasksMeter;

struct TasksMeter_ {
   Meter super;
   ProcessList* pl;
};

}*/

TasksMeter* TasksMeter_new(ProcessList* pl) {
   TasksMeter* this = malloc(sizeof(TasksMeter));
   Meter_init((Meter*)this, String_copy("Tasks"), String_copy("Tasks: "), 1);
   ((Meter*)this)->attributes[0] = TASKS_RUNNING;
   ((Object*)this)->display = TasksMeter_display;
   ((Meter*)this)->setValues = TasksMeter_setValues;
   this->pl = pl;
   Meter_setMode((Meter*)this, TEXT);
   return this;
}

void TasksMeter_setValues(Meter* cast) {
   TasksMeter* this = (TasksMeter*)cast;
   cast->total = this->pl->totalTasks;
   cast->values[0] = this->pl->runningTasks;
   snprintf(cast->displayBuffer.c, 20, "%d/%d", (int) cast->values[0], (int) cast->total);
}

void TasksMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   RichString_prune(out);
   char buffer[20];
   sprintf(buffer, "%d", (int)this->total);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " total, ");
   sprintf(buffer, "%d", (int)this->values[0]);
   RichString_append(out, CRT_colors[TASKS_RUNNING], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " running");
}
