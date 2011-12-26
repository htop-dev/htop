/*
htop - TasksMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TasksMeter.h"

#include "ProcessList.h"
#include "CRT.h"

/*{
#include "Meter.h"
}*/

int TasksMeter_attributes[] = {
   TASKS_RUNNING
};

static void TasksMeter_setValues(Meter* this, char* buffer, int len) {
   ProcessList* pl = this->pl;
   this->total = pl->totalTasks;
   this->values[0] = pl->runningTasks;
   snprintf(buffer, len, "%d/%d", (int) this->values[0], (int) this->total);
}

static void TasksMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   ProcessList* pl = this->pl;
   char buffer[20];
   sprintf(buffer, "%d", (int)(this->total - pl->userlandThreads - pl->kernelThreads));
   RichString_write(out, CRT_colors[METER_VALUE], buffer);
   int threadValueColor = CRT_colors[METER_VALUE];
   int threadCaptionColor = CRT_colors[METER_TEXT];
   if (pl->highlightThreads) {
      threadValueColor = CRT_colors[PROCESS_THREAD_BASENAME];
      threadCaptionColor = CRT_colors[PROCESS_THREAD];
   }
   if (!pl->hideUserlandThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      sprintf(buffer, "%d", (int)pl->userlandThreads);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " thr");
   }
   if (!pl->hideKernelThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      sprintf(buffer, "%d", (int)pl->kernelThreads);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " kthr");
   }
   RichString_append(out, CRT_colors[METER_TEXT], "; ");
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
