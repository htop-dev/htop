/*
htop - TasksMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TasksMeter.h"

#include "Platform.h"
#include "CRT.h"


int TasksMeter_attributes[] = {
   CPU_SYSTEM, PROCESS_THREAD, PROCESS, TASKS_RUNNING
};

static void TasksMeter_updateValues(Meter* this, char* buffer, int len) {
   ProcessList* pl = this->pl;
   this->values[0] = pl->kernelThreads;
   this->values[1] = pl->userlandThreads;
   this->values[2] = pl->totalTasks - pl->kernelThreads - pl->userlandThreads;
   this->values[3] = MINIMUM(pl->runningTasks, pl->cpuCount);
   if (pl->totalTasks > this->total) {
      this->total = pl->totalTasks;
   }
   if (this->pl->settings->hideKernelThreads) {
      this->values[0] = 0;
   }
   xSnprintf(buffer, len, "%d/%d", (int) this->values[3], (int) this->total);
}

static void TasksMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   Settings* settings = this->pl->settings;
   char buffer[20];

   int processes = (int) this->values[2];

   xSnprintf(buffer, sizeof(buffer), "%d", processes);
   RichString_write(out, CRT_colors[METER_VALUE], buffer);
   int threadValueColor = CRT_colors[METER_VALUE];
   int threadCaptionColor = CRT_colors[METER_TEXT];
   if (settings->highlightThreads) {
      threadValueColor = CRT_colors[PROCESS_THREAD_BASENAME];
      threadCaptionColor = CRT_colors[PROCESS_THREAD];
   }
   if (!settings->hideUserlandThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[1]);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " thr");
   }
   if (!settings->hideKernelThreads) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[0]);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " kthr");
   }
   RichString_append(out, CRT_colors[METER_TEXT], "; ");
   xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[3]);
   RichString_append(out, CRT_colors[TASKS_RUNNING], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " running");
}

MeterClass TasksMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = TasksMeter_display,
   },
   .updateValues = TasksMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 4,
   .total = 100.0,
   .attributes = TasksMeter_attributes,
   .name = "Tasks",
   .uiName = "Task counter",
   .caption = "Tasks: "
};
