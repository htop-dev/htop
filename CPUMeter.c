/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CPUMeter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static const int CPUMeter_attributes[] = {
   CPU_NICE,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   CPU_IOWAIT
};

typedef struct CPUMeterData_ {
   unsigned int cpus;
   Meter** meters;
} CPUMeterData;

static void CPUMeter_init(Meter* this) {
   unsigned int cpu = this->param;
   if (cpu == 0) {
      Meter_setCaption(this, "Avg");
   } else if (this->pl->activeCPUs > 1) {
      char caption[10];
      xSnprintf(caption, sizeof(caption), "%3u", Settings_cpuId(this->pl->settings, cpu - 1));
      Meter_setCaption(this, caption);
   }
}

// Custom uiName runtime logic to include the param (processor)
static void CPUMeter_getUiName(const Meter* this, char* buffer, size_t length) {
   if (this->param > 0)
      xSnprintf(buffer, length, "%s %u", Meter_uiName(this), this->param);
   else
      xSnprintf(buffer, length, "%s", Meter_uiName(this));
}

static void CPUMeter_updateValues(Meter* this) {
   memset(this->values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);

   unsigned int cpu = this->param;
   if (cpu > this->pl->existingCPUs) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "absent");
      return;
   }

   double percent = Platform_setCPUValues(this, cpu);
   if (isnan(percent)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "offline");
      return;
   }

   char cpuUsageBuffer[8] = { 0 };
   char cpuFrequencyBuffer[16] = { 0 };
   char cpuTemperatureBuffer[16] = { 0 };

   if (this->pl->settings->showCPUUsage) {
      xSnprintf(cpuUsageBuffer, sizeof(cpuUsageBuffer), "%.1f%%", percent);
   }

   if (this->pl->settings->showCPUFrequency) {
      double cpuFrequency = this->values[CPU_METER_FREQUENCY];
      if (isnan(cpuFrequency)) {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "N/A");
      } else {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "%4uMHz", (unsigned)cpuFrequency);
      }
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (this->pl->settings->showCPUTemperature) {
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isnan(cpuTemperature))
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      else if (this->pl->settings->degreeFahrenheit)
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%3d%sF", (int)(cpuTemperature * 9 / 5 + 32), CRT_degreeSign);
      else
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%d%sC", (int)cpuTemperature, CRT_degreeSign);
   }
   #endif

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s%s%s%s",
             cpuUsageBuffer,
             (cpuUsageBuffer[0] && (cpuFrequencyBuffer[0] || cpuTemperatureBuffer[0])) ? " " : "",
             cpuFrequencyBuffer,
             (cpuFrequencyBuffer[0] && cpuTemperatureBuffer[0]) ? " " : "",
             cpuTemperatureBuffer);
}

static void CPUMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   int len;
   const Meter* this = (const Meter*)cast;

   if (this->param > this->pl->existingCPUs) {
      RichString_appendAscii(out, CRT_colors[METER_SHADOW], " absent");
      return;
   }

   if (this->curItems == 0) {
      RichString_appendAscii(out, CRT_colors[METER_SHADOW], " offline");
      return;
   }

   len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NORMAL]);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], ":");
   RichString_appendnAscii(out, CRT_colors[CPU_NORMAL], buffer, len);
   if (this->pl->settings->detailedCPUTime) {
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sy:");
      RichString_appendnAscii(out, CRT_colors[CPU_SYSTEM], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "ni:");
      RichString_appendnAscii(out, CRT_colors[CPU_NICE_TEXT], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "hi:");
      RichString_appendnAscii(out, CRT_colors[CPU_IRQ], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_SOFTIRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "si:");
      RichString_appendnAscii(out, CRT_colors[CPU_SOFTIRQ], buffer, len);
      if (!isnan(this->values[CPU_METER_STEAL])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_STEAL]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "st:");
         RichString_appendnAscii(out, CRT_colors[CPU_STEAL], buffer, len);
      }
      if (!isnan(this->values[CPU_METER_GUEST])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_GUEST]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "gu:");
         RichString_appendnAscii(out, CRT_colors[CPU_GUEST], buffer, len);
      }
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IOWAIT]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "wa:");
      RichString_appendnAscii(out, CRT_colors[CPU_IOWAIT], buffer, len);
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sys:");
      RichString_appendnAscii(out, CRT_colors[CPU_SYSTEM], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "low:");
      RichString_appendnAscii(out, CRT_colors[CPU_NICE_TEXT], buffer, len);
      if (!isnan(this->values[CPU_METER_IRQ])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "vir:");
         RichString_appendnAscii(out, CRT_colors[CPU_GUEST], buffer, len);
      }
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (this->pl->settings->showCPUTemperature) {
      char cpuTemperatureBuffer[10];
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isnan(cpuTemperature)) {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      } else if (this->pl->settings->degreeFahrenheit) {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sF", cpuTemperature * 9 / 5 + 32, CRT_degreeSign);
      } else {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sC", cpuTemperature, CRT_degreeSign);
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "temp:");
      RichString_appendnWide(out, CRT_colors[METER_VALUE], cpuTemperatureBuffer, len);
   }
   #endif
}

static void AllCPUsMeter_getRange(const Meter* this, int* start, int* count) {
   const CPUMeterData* data = this->meterData;
   unsigned int cpus = data->cpus;
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

static void AllCPUsMeter_updateValues(Meter* this) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_updateValues(meters[i]);
}

static void CPUMeterCommonInit(Meter* this, int ncol) {
   unsigned int cpus = this->pl->existingCPUs;
   CPUMeterData* data = this->meterData;
   if (!data) {
      data = this->meterData = xMalloc(sizeof(CPUMeterData));
      data->cpus = cpus;
      data->meters = xCalloc(cpus, sizeof(Meter*));
   }
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->pl, start + i + 1, (const MeterClass*) Class(CPUMeter));

      Meter_init(meters[i]);
   }

   if (this->mode == 0)
      this->mode = BAR_METERMODE;

   int h = Meter_modes[this->mode]->h;
   this->h = h * ((count + ncol - 1) / ncol);
}

static void CPUMeterCommonUpdateMode(Meter* this, int mode, int ncol) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   this->h = h * ((count + ncol - 1) / ncol);
}

static void AllCPUsMeter_done(Meter* this) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
   free(data->meters);
   free(data);
}

static void SingleColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 1);
}

static void SingleColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 1);
}

static void DualColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 2);
}

static void DualColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 2);
}

static void QuadColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 4);
}

static void QuadColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 4);
}

static void OctoColCPUsMeter_init(Meter* this) {
   CPUMeterCommonInit(this, 8);
}

static void OctoColCPUsMeter_updateMode(Meter* this, int mode) {
   CPUMeterCommonUpdateMode(this, mode, 8);
}

static void CPUMeterCommonDraw(Meter* this, int x, int y, int w, int ncol) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   int colwidth = (w - ncol) / ncol + 1;
   int diff = (w - (colwidth * ncol));
   int nrows = (count + ncol - 1) / ncol;
   for (int i = 0; i < count; i++) {
      int d = (i / nrows) > diff ? diff : (i / nrows); // dynamic spacer
      int xpos = x + ((i / nrows) * colwidth) + d;
      int ypos = y + ((i % nrows) * meters[0]->h);
      meters[i]->draw(meters[i], xpos, ypos, colwidth);
   }
}

static void DualColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 2);
}

static void QuadColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 4);
}

static void OctoColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterCommonDraw(this, x, y, w, 8);
}


static void SingleColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   CPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}


const MeterClass CPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = CPUMeter_updateValues,
   .getUiName = CPUMeter_getUiName,
   .defaultMode = BAR_METERMODE,
   .maxItems = CPU_METER_ITEMCOUNT,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

const MeterClass AllCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs",
   .uiName = "CPUs (1/1)",
   .description = "CPUs (1/1): all CPUs",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .description = "CPUs (1&2/2): all CPUs in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .description = "CPUs (1/2): first half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .description = "CPUs (2/2): second half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = SingleColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .description = "CPUs (1&2/4): first half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .description = "CPUs (3&4/4): second half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = DualColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs4",
   .uiName = "CPUs (1&2&3&4/4)",
   .description = "CPUs (1&2&3&4/4): all CPUs in 4 shorter columns",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs4",
   .uiName = "CPUs (1-4/8)",
   .description = "CPUs (1-4/8): first half in 4 shorter columns",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs4",
   .uiName = "CPUs (5-8/8)",
   .description = "CPUs (5-8/8): second half in 4 shorter columns",
   .caption = "CPU",
   .draw = QuadColCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = QuadColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs8",
   .uiName = "CPUs (1-8/8)",
   .description = "CPUs (1-8/8): all CPUs in 8 shorter columns",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs8",
   .uiName = "CPUs (1-8/16)",
   .description = "CPUs (1-8/16): first half in 8 shorter columns",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs8",
   .uiName = "CPUs (9-16/16)",
   .description = "CPUs (9-16/16): second half in 8 shorter columns",
   .caption = "CPU",
   .draw = OctoColCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = OctoColCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};
