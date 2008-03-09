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

int TasksMeter_attributes[] = {
   TASKS_RUNNING
};

static void TasksMeter_setValues(Meter* this, char* buffer, int len) {
   this->total = this->pl->totalTasks;
   this->values[0] = this->pl->runningTasks;
   snprintf(buffer, len, "%d/%d", (int) this->values[0], (int) this->total);
}

static void TasksMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   RichString_init(out);
   char buffer[20];
   sprintf(buffer, "%d", (int)this->total);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " total, ");
   sprintf(buffer, "%d", (int)this->values[0]);
   RichString_append(out, CRT_colors[TASKS_RUNNING], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " running");
}

MeterType TasksMeter = {
   .setValues = TasksMeter_setValues, 
   .display = TasksMeter_display,
   .mode = TEXT_METERMODE,
   .items = 1,
   .total = 100.0,
   .attributes = TasksMeter_attributes, 
   .name = "Tasks",
   .uiName = "Task counter",
   .caption = "Tasks: "
};
