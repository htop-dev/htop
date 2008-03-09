/*
htop - CPUMeter.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>

#include "debug.h"
#include <assert.h>

int CPUMeter_attributes[] = {
   CPU_NICE, CPU_NORMAL, CPU_KERNEL, CPU_IRQ, CPU_SOFTIRQ, CPU_IOWAIT
};

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static void CPUMeter_init(Meter* this) {
   int processor = this->param;
   if (this->pl->processorCount > 1) {
      char caption[10];
      sprintf(caption, "%-3d", processor);
      Meter_setCaption(this, caption);
   }
   if (this->param == 0)
      Meter_setCaption(this, "Avg");
}

static void CPUMeter_setValues(Meter* this, char* buffer, int size) {
   ProcessList* pl = this->pl;
   int processor = this->param;
   double total = (double) pl->totalPeriod[processor];
   double cpu;
   this->values[0] = pl->nicePeriod[processor] / total * 100.0;
   this->values[1] = pl->userPeriod[processor] / total * 100.0;
   if (pl->detailedCPUTime) {
      this->values[2] = pl->systemPeriod[processor] / total * 100.0;
      this->values[3] = pl->irqPeriod[processor] / total * 100.0;
      this->values[4] = pl->softIrqPeriod[processor] / total * 100.0;
      this->values[5] = pl->ioWaitPeriod[processor] / total * 100.0;
      this->type->items = 6;
      cpu = MIN(100.0, MAX(0.0, (this->values[0]+this->values[1]+this->values[2]+
                       this->values[3]+this->values[4])));
   } else {
      this->values[2] = pl->systemAllPeriod[processor] / total * 100.0;
      this->type->items = 3;
      cpu = MIN(100.0, MAX(0.0, (this->values[0]+this->values[1]+this->values[2])));
   }
   snprintf(buffer, size, "%5.1f%%", cpu );
}

static void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_init(out);
   sprintf(buffer, "%5.1f%% ", this->values[1]);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   RichString_append(out, CRT_colors[CPU_NORMAL], buffer);
   if (this->pl->detailedCPUTime) {
      sprintf(buffer, "%5.1f%% ", this->values[2]);
      RichString_append(out, CRT_colors[METER_TEXT], "sy:");
      RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[0]);
      RichString_append(out, CRT_colors[METER_TEXT], "ni:");
      RichString_append(out, CRT_colors[CPU_NICE], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[3]);
      RichString_append(out, CRT_colors[METER_TEXT], "hi:");
      RichString_append(out, CRT_colors[CPU_IRQ], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[4]);
      RichString_append(out, CRT_colors[METER_TEXT], "si:");
      RichString_append(out, CRT_colors[CPU_SOFTIRQ], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[5]);
      RichString_append(out, CRT_colors[METER_TEXT], "wa:");
      RichString_append(out, CRT_colors[CPU_IOWAIT], buffer);
   } else {
      sprintf(buffer, "%5.1f%% ", this->values[2]);
      RichString_append(out, CRT_colors[METER_TEXT], "sys:");
      RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[0]);
      RichString_append(out, CRT_colors[METER_TEXT], "low:");
      RichString_append(out, CRT_colors[CPU_NICE], buffer);
   }
}

static void AllCPUsMeter_init(Meter* this) {
   int processors = this->pl->processorCount;
   this->drawBuffer = malloc(sizeof(Meter*) * processors);
   Meter** meters = (Meter**) this->drawBuffer;
   for (int i = 0; i < processors; i++)
      meters[i] = Meter_new(this->pl, i+1, &CPUMeter);
   this->h = processors;
   this->mode = BAR_METERMODE;
}

static void AllCPUsMeter_done(Meter* this) {
   int processors = this->pl->processorCount;
   Meter** meters = (Meter**) this->drawBuffer;
   for (int i = 0; i < processors; i++)
      Meter_delete((Object*)meters[i]);
}

static void AllCPUsMeter_setMode(Meter* this, int mode) {
   this->mode = mode;
   int processors = this->pl->processorCount;
   int h = Meter_modes[this->mode]->h;
   this->h = h * processors;
}

static void AllCPUsMeter_draw(Meter* this, int x, int y, int w) {
   int processors = this->pl->processorCount;
   Meter** meters = (Meter**) this->drawBuffer;
   for (int i = 0; i < processors; i++) {
      Meter_setMode(meters[i], this->mode);
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}

MeterType CPUMeter = {
   .setValues = CPUMeter_setValues, 
   .display = CPUMeter_display,
   .mode = BAR_METERMODE,
   .items = 6,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

MeterType AllCPUsMeter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "AllCPUs",
   .uiName = "All CPUs",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};
