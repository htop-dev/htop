/*
htop - TasksMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "TasksMeter.h"

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "ProcessTable.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static const int TasksMeter_attributes[] = {
   CPU_SYSTEM,
   PROCESS_THREAD,
   PROCESS,
   TASKS_RUNNING
};

static void TasksMeter_updateValues(Meter* this) {
   const Machine* host = this->host;
   const ProcessTable* pt = (const ProcessTable*) host->processTable;

   this->values[0] = pt->kernelThreads;
   this->values[1] = pt->userlandThreads;
   this->values[2] = pt->totalTasks - pt->kernelThreads - pt->userlandThreads;
   this->values[3] = MINIMUM(pt->runningTasks, host->activeCPUs);
   this->total     = pt->totalTasks;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%u/%u", MINIMUM(pt->runningTasks, host->activeCPUs), pt->totalTasks);
}

static void TasksMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   const Settings* settings = this->host->settings;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[2]);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);

   RichString_appendAscii(out, settings->hideUserlandThreads ? CRT_colors[METER_SHADOW] : CRT_colors[METER_TEXT], ", ");
   len = xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[1]);
   RichString_appendnAscii(out, settings->hideUserlandThreads ? CRT_colors[METER_SHADOW] : CRT_colors[TASKS_RUNNING], buffer, len);
   RichString_appendAscii(out, settings->hideUserlandThreads ? CRT_colors[METER_SHADOW] : CRT_colors[METER_TEXT], " thr");

   RichString_appendAscii(out, settings->hideKernelThreads ? CRT_colors[METER_SHADOW] : CRT_colors[METER_TEXT], ", ");
   len = xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[0]);
   RichString_appendnAscii(out, settings->hideKernelThreads ? CRT_colors[METER_SHADOW] : CRT_colors[TASKS_RUNNING], buffer, len);
   RichString_appendAscii(out, settings->hideKernelThreads ? CRT_colors[METER_SHADOW] : CRT_colors[METER_TEXT], " kthr");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "; ");
   len = xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[3]);
   RichString_appendnAscii(out, CRT_colors[TASKS_RUNNING], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " running");
}

const MeterClass TasksMeter_class = {
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
