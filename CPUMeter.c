/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"

#include "CRT.h"
#include "Settings.h"
#include "Platform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int CPUMeter_attributes[] = {
   CPU_NICE, CPU_NORMAL, CPU_SYSTEM, CPU_IRQ, CPU_SOFTIRQ, CPU_STEAL, CPU_GUEST, CPU_IOWAIT
};

static void CPUMeter_init(Meter* this) {
   int cpu = this->param;
   if (this->pl->cpuCount > 1) {
      char caption[10];
      xSnprintf(caption, sizeof(caption), "%-3d", Settings_cpuId(this->pl->settings, cpu - 1));
      Meter_setCaption(this, caption);
   }
   if (this->param == 0)
      Meter_setCaption(this, "Avg");
}

static void CPUMeter_updateValues(Meter* this, char* buffer, int size) {
   int cpu = this->param;
   if (cpu > this->pl->cpuCount) {
      xSnprintf(buffer, size, "absent");
      return;
   }
   memset(this->values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);
   double percent = Platform_setCPUValues(this, cpu);
   if (this->pl->settings->showCPUFrequency) {
      double cpuFrequency = this->values[CPU_METER_FREQUENCY];
      char cpuFrequencyBuffer[16];
      if (cpuFrequency < 0) {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "N/A");
      } else {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "%.0fMHz", cpuFrequency);
      }
      if (this->pl->settings->showCPUUsage) {
         xSnprintf(buffer, size, "%5.1f%% %s", percent, cpuFrequencyBuffer);
      } else {
         xSnprintf(buffer, size, "%s", cpuFrequencyBuffer);
      }
   } else if (this->pl->settings->showCPUUsage) {
      xSnprintf(buffer, size, "%5.1f%%", percent);
   } else if (size > 0) {
      buffer[0] = '\0';
   }
}

static void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_prune(out);
   if (this->param > this->pl->cpuCount) {
      RichString_append(out, CRT_colors[METER_TEXT], "absent");
      return;
   }
   xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NORMAL]);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   RichString_append(out, CRT_colors[CPU_NORMAL], buffer);
   if (this->pl->settings->detailedCPUTime) {
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_append(out, CRT_colors[METER_TEXT], "sy:");
      RichString_append(out, CRT_colors[CPU_SYSTEM], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_append(out, CRT_colors[METER_TEXT], "ni:");
      RichString_append(out, CRT_colors[CPU_NICE_TEXT], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
      RichString_append(out, CRT_colors[METER_TEXT], "hi:");
      RichString_append(out, CRT_colors[CPU_IRQ], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_SOFTIRQ]);
      RichString_append(out, CRT_colors[METER_TEXT], "si:");
      RichString_append(out, CRT_colors[CPU_SOFTIRQ], buffer);
      if (this->values[CPU_METER_STEAL]) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_STEAL]);
         RichString_append(out, CRT_colors[METER_TEXT], "st:");
         RichString_append(out, CRT_colors[CPU_STEAL], buffer);
      }
      if (this->values[CPU_METER_GUEST]) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_GUEST]);
         RichString_append(out, CRT_colors[METER_TEXT], "gu:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IOWAIT]);
      RichString_append(out, CRT_colors[METER_TEXT], "wa:");
      RichString_append(out, CRT_colors[CPU_IOWAIT], buffer);
   } else {
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_append(out, CRT_colors[METER_TEXT], "sys:");
      RichString_append(out, CRT_colors[CPU_SYSTEM], buffer);
      xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_append(out, CRT_colors[METER_TEXT], "low:");
      RichString_append(out, CRT_colors[CPU_NICE_TEXT], buffer);
      if (this->values[CPU_METER_IRQ]) {
         xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
         RichString_append(out, CRT_colors[METER_TEXT], "vir:");
         RichString_append(out, CRT_colors[CPU_GUEST], buffer);
      }
   }
}

static void AllCPUsMeter_getRange(Meter* this, int* start, int* count) {
   int cpus = this->pl->cpuCount;
   switch(Meter_name(this)[0]) {
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

static int MapClassnameToColumncount(Meter* this){
   if (strchr(Meter_name(this), '4'))
      return 4;
   else if (strchr(Meter_name(this), '2'))
      return 2;
   else
      return 1;
}

static void AllCPUsMeter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
   if (!this->drawData)
      this->drawData = xCalloc(cpus, sizeof(Meter*));
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->pl, start+i+1, (MeterClass*) Class(CPUMeter));
      Meter_init(meters[i]);
   }
   if (this->mode == 0)
      this->mode = BAR_METERMODE;
   int h = Meter_modes[this->mode]->h;
   int ncol = MapClassnameToColumncount(this);
   this->h = h * ((count + ncol - 1)/ ncol);
}

static void AllCPUsMeter_done(Meter* this) {
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
}

static void AllCPUsMeter_updateMode(Meter* this, int mode) {
   Meter** meters = (Meter**) this->drawData;
   this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   int ncol = MapClassnameToColumncount(this);
   this->h = h * ((count + ncol - 1)/ ncol);
}

static void DualColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   Meter** meters = (Meter**) this->drawData;
   int start, count;
   int pad = this->pl->settings->headerMargin ? 2 : 0;
   AllCPUsMeter_getRange(this, &start, &count);
   int height = (count+1)/2;
   int startY = y;
   for (int i = 0; i < height; i++) {
      meters[i]->draw(meters[i], x, y, (w-pad)/2);
      y += meters[i]->h;
   }
   y = startY;
   for (int i = height; i < count; i++) {
      meters[i]->draw(meters[i], x+(w-1)/2+1+(pad/2), y, (w-pad)/2);
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

static void MultiColCPUsMeter_draw(Meter* this, int x, int y, int w){
  Meter** meters = (Meter**) this->drawData;
  int start, count;
  AllCPUsMeter_getRange(this, &start, &count);
  int ncol = MapClassnameToColumncount(this);
  int colwidth = (w-ncol)/ncol + 1;
  int diff = (w - (colwidth * ncol));
  int nrows = (count + ncol - 1) / ncol;
  for (int i = 0; i < count; i++){
    int d = (i/nrows) > diff ? diff : (i / nrows) ; // dynamic spacer
    int xpos = x + ((i / nrows) * colwidth) + d;
    int ypos = y + ((i % nrows) * meters[0]->h);
    meters[i]->draw(meters[i], xpos, ypos, colwidth);
  }
}

MeterClass CPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = CPUMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = CPU_METER_ITEMCOUNT,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

MeterClass AllCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs",
   .uiName = "CPUs (1/1)",
   .description = "CPUs (1/1): all CPUs",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .description = "CPUs (1&2/2): all CPUs in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .description = "CPUs (1/2): first half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .description = "CPUs (2/2): second half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .description = "CPUs (1&2/4): first half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .description = "CPUs (3&4/4): second half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass AllCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs4",
   .uiName = "CPUs (1&2&3&4/4)",
   .description = "CPUs (1&2&3&4/4): all CPUs in 4 shorter columns",
   .caption = "CPU",
   .draw = MultiColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs4",
   .uiName = "CPUs (1-4/8)",
   .description = "CPUs (1-4/8): first half in 4 shorter columns",
   .caption = "CPU",
   .draw = MultiColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs4",
   .uiName = "CPUs (5-8/8)",
   .description = "CPUs (5-8/8): second half in 4 shorter columns",
   .caption = "CPU",
   .draw = MultiColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};
