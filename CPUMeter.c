/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"

#include "CRT.h"
#include "ProcessList.h"

#include <assert.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>

/*{
#include "Meter.h"
}*/

int CPUMeter_attributes[] = {
   CPU_NICE, CPU_NORMAL, CPU_KERNEL, CPU_IRQ, CPU_SOFTIRQ, CPU_IOWAIT, CPU_STEAL, CPU_GUEST
};

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static void CPUMeter_init(Meter* this) {
   int cpu = this->param;
   if (this->pl->cpuCount > 1) {
      char caption[10];
      sprintf(caption, "%-3d", ProcessList_cpuId(this->pl, cpu - 1));
      Meter_setCaption(this, caption);
   }
   if (this->param == 0)
      Meter_setCaption(this, "Avg");
}

static void CPUMeter_setValues(Meter* this, char* buffer, int size) {
   ProcessList* pl = this->pl;
   int cpu = this->param;
   if (cpu > this->pl->cpuCount) {
      snprintf(buffer, size, "absent");
      return;
   }
   CPUData* cpuData = &(pl->cpus[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   this->values[0] = cpuData->nicePeriod / total * 100.0;
   this->values[1] = cpuData->userPeriod / total * 100.0;
   if (pl->detailedCPUTime) {
      this->values[2] = cpuData->systemPeriod / total * 100.0;
      this->values[3] = cpuData->irqPeriod / total * 100.0;
      this->values[4] = cpuData->softIrqPeriod / total * 100.0;
      this->values[5] = cpuData->ioWaitPeriod / total * 100.0;
      this->values[6] = cpuData->stealPeriod / total * 100.0;
      this->values[7] = cpuData->guestPeriod / total * 100.0;
      this->type->items = 8;
      percent = MIN(100.0, MAX(0.0, (this->values[0]+this->values[1]+this->values[2]+
                       this->values[3]+this->values[4])));
   } else {
      this->values[2] = cpuData->systemAllPeriod / total * 100.0;
      this->values[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      this->type->items = 4;
      percent = MIN(100.0, MAX(0.0, (this->values[0]+this->values[1]+this->values[2]+this->values[3])));
   }
   if (isnan(percent)) percent = 0.0;
   snprintf(buffer, size, "%5.1f%%", percent);
}

static void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_prune(out);
   if (this->param > this->pl->cpuCount) {
      RichString_append(out, CRT_colors[METER_TEXT], "absent");
      return;
   }
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
      sprintf(buffer, "%5.1f%% ", this->values[6]);
      RichString_append(out, CRT_colors[METER_TEXT], "st:");
      RichString_append(out, CRT_colors[CPU_STEAL], buffer);
      if (this->values[7]) {
         sprintf(buffer, "%5.1f%% ", this->values[7]);
         RichString_append(out, CRT_colors[METER_TEXT], "gu:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
   } else {
      sprintf(buffer, "%5.1f%% ", this->values[2]);
      RichString_append(out, CRT_colors[METER_TEXT], "sys:");
      RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
      sprintf(buffer, "%5.1f%% ", this->values[0]);
      RichString_append(out, CRT_colors[METER_TEXT], "low:");
      RichString_append(out, CRT_colors[CPU_NICE], buffer);
      if (this->values[3]) {
         sprintf(buffer, "%5.1f%% ", this->values[3]);
         RichString_append(out, CRT_colors[METER_TEXT], "vir:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
   }
}

static void AllCPUsMeter_getRange(Meter* this, int* start, int* count) {
   int cpus = this->pl->cpuCount;
   switch(this->type->name[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = cpus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (cpus+1) / 2;
         break;
      case 'R': // Second Half
         *start = (cpus+1) / 2;
         *count = cpus / 2;
         break;
   }
}

static void AllCPUsMeter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
   if (!this->drawData)
      this->drawData = calloc(sizeof(Meter*), cpus);
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->pl, start+i+1, &CPUMeter);
      meters[i]->type->init(meters[i]);
   }
   if (this->mode == 0)
      this->mode = BAR_METERMODE;
   int h = Meter_modes[this->mode]->h;
   if (strchr(this->type->name, '2'))
      this->h = h * ((count+1) / 2);
   else
      this->h = h * count;
}

static void AllCPUsMeter_done(Meter* this) {
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
}

static void AllCPUsMeter_setMode(Meter* this, int mode) {
   Meter** meters = (Meter**) this->drawData;
   this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   if (strchr(this->type->name, '2'))
      this->h = h * ((count+1) / 2);
   else
      this->h = h * count;
}

static void DualColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   int height = (count+1)/2;
   int startY = y;
   for (int i = 0; i < height; i++) {
      meters[i]->draw(meters[i], x, y, (w-2)/2);
      y += meters[i]->h;
   }
   y = startY;
   for (int i = height; i < count; i++) {
      meters[i]->draw(meters[i], x+(w-1)/2+2, y, (w-2)/2);
      y += meters[i]->h;
   }
}

static void SingleColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}

MeterType CPUMeter = {
   .setValues = CPUMeter_setValues, 
   .display = CPUMeter_display,
   .mode = BAR_METERMODE,
   .items = 8,
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
   .uiName = "CPUs (1/1)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

MeterType AllCPUs2Meter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

MeterType LeftCPUsMeter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

MeterType RightCPUsMeter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

MeterType LeftCPUs2Meter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

MeterType RightCPUs2Meter = {
   .mode = 0,
   .items = 1,
   .total = 100.0,
   .attributes = CPUMeter_attributes, 
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .setMode = AllCPUsMeter_setMode,
   .done = AllCPUsMeter_done
};

