/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CPUMeter.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
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

static const int CPUMeter_attributes_summary[] = {
   CPU_NICE,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_GUEST
};

typedef struct CPUMeterData_ {
   unsigned int ncol;         /* columns the shown sub-meters are laid out in */
   unsigned int cpus;         /* size of meters: one slot per existing CPU */
   Meter** meters;            /* sub-meter per CPU id, created when first shown */
   unsigned int shownCount;
   Meter** shown;             /* the sub-meters currently shown, in display order */
} CPUMeterData;

static void CPUMeter_init(Meter* this) {
   unsigned int cpu = this->param;
   const Machine* host = this->host;
   if (cpu == 0) {
      Meter_setCaption(this, "Avg");
   } else if (host->activeCPUs > 1) {
      char caption[10];
      /* The SMT topology of a CPU that is not present cannot be looked up,
         so fall back to the plain CPU number for such a meter. */
      if (host->settings->showCPUSMTLabels && cpu <= host->existingCPUs) {
         int coreID = Machine_getCPUPhysicalCoreID(host, cpu - 1);
         int threadIndex = Machine_getCPUThreadIndex(host, cpu - 1);
         char threadLetter = 'a' + (char)(threadIndex % 26);
         // if we have more than 26 threads per core, then add the capital
         // letters into the mix. If we have more than 52 threads per core, then
         // some letters will still be repeated, but they'll be far apart from
         // each other.
         if ((threadIndex % 52) > 26) {
             threadLetter -= ('a' - 'A');
         }
         xSnprintf(caption, sizeof(caption), "%2d%c", Settings_cpuId(host->settings, coreID), threadLetter);
      } else {
         xSnprintf(caption, sizeof(caption), "%3u", Settings_cpuId(host->settings, cpu - 1));
      }
      Meter_setCaption(this, caption);
   }
}

// Custom uiName runtime logic to include the param (processor)
static void CPUMeter_getUiName(const Meter* this, char* buffer, size_t length) {
   assert(length > 0);

   if (this->param > 0)
      xSnprintf(buffer, length, "%s %u", Meter_uiName(this), Settings_cpuId(this->host->settings, this->param - 1));
   else
      xSnprintf(buffer, length, "%s", Meter_uiName(this));
}

static void CPUMeter_updateValues(Meter* this) {
   memset(this->values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);

   const Machine* host = this->host;
   const Settings* settings = host->settings;
   if (settings->detailedCPUTime) {
      this->curAttributes = CPUMeter_attributes;
   } else {
      this->curAttributes = CPUMeter_attributes_summary;
   }

   unsigned int cpu = this->param;
   if (cpu > host->existingCPUs) {
      /* Nothing to draw: leaving curItems at its Meter_new() default of
         maxItems would make the bar and graph modes walk past the end of
         curAttributes (4 or 8 entries, while maxItems is CPU_METER_ITEMCOUNT). */
      this->curItems = 0;
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "absent");
      return;
   }

   double percent = Platform_setCPUValues(this, cpu);
   if (!isNonnegative(percent)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "offline");
      return;
   }

   char cpuUsageBuffer[8] = { 0 };
   char cpuFrequencyBuffer[16] = { 0 };
   char cpuTemperatureBuffer[16] = { 0 };

   if (settings->showCPUUsage) {
      xSnprintf(cpuUsageBuffer, sizeof(cpuUsageBuffer), "%.1f%%", percent);
   }

   if (settings->showCPUFrequency) {
      double cpuFrequency = this->values[CPU_METER_FREQUENCY];
      if (isNonnegative(cpuFrequency)) {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "%4uMHz", (unsigned)cpuFrequency);
      } else {
         xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "N/A");
      }
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (settings->showCPUTemperature) {
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isNaN(cpuTemperature))
         xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      else if (settings->degreeFahrenheit)
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
   const Machine* host = this->host;
   const Settings* settings = host->settings;

   if (this->param > host->existingCPUs) {
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
   if (settings->detailedCPUTime) {
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
      if (isNonnegative(this->values[CPU_METER_STEAL])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_STEAL]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "st:");
         RichString_appendnAscii(out, CRT_colors[CPU_STEAL], buffer, len);
      }
      if (isNonnegative(this->values[CPU_METER_GUEST])) {
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
      if (isNonnegative(this->values[CPU_METER_IRQ])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[CPU_METER_IRQ]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "vir:");
         RichString_appendnAscii(out, CRT_colors[CPU_GUEST], buffer, len);
      }
   }

   if (settings->showCPUFrequency) {
      char cpuFrequencyBuffer[10];
      double cpuFrequency = this->values[CPU_METER_FREQUENCY];
      if (isNonnegative(cpuFrequency)) {
         len = xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "%4uMHz ", (unsigned)cpuFrequency);
      } else {
         len = xSnprintf(cpuFrequencyBuffer, sizeof(cpuFrequencyBuffer), "N/A     ");
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "freq: ");
      RichString_appendnWide(out, CRT_colors[METER_VALUE], cpuFrequencyBuffer, len);
   }

   #ifdef BUILD_WITH_CPU_TEMP
   if (settings->showCPUTemperature) {
      char cpuTemperatureBuffer[10];
      double cpuTemperature = this->values[CPU_METER_TEMPERATURE];
      if (isNaN(cpuTemperature)) {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "N/A");
      } else if (settings->degreeFahrenheit) {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sF", cpuTemperature * 9 / 5 + 32, CRT_degreeSign);
      } else {
         len = xSnprintf(cpuTemperatureBuffer, sizeof(cpuTemperatureBuffer), "%5.1f%sC", cpuTemperature, CRT_degreeSign);
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "temp:");
      RichString_appendnWide(out, CRT_colors[METER_VALUE], cpuTemperatureBuffer, len);
   }
   #endif
}

/* Whether the multi-CPU meters show a slot for this CPU (0-based id) */
static bool CPUMeter_isShown(const Machine* host, unsigned int cpu) {
   return !host->settings->hideOfflineCPUs || Machine_isCPUonline(host, cpu);
}

static unsigned int CPUMeter_countShown(const Machine* host) {
   unsigned int shown = 0;
   for (unsigned int cpu = 0; cpu < host->existingCPUs; cpu++) {
      if (CPUMeter_isShown(host, cpu))
         shown++;
   }
   return shown;
}

/* Range of this meter within the sequence of shown CPUs */
static void AllCPUsMeter_getRange(const Meter* this, unsigned int* start, unsigned int* count) {
   unsigned int cpus = CPUMeter_countShown(this->host);
   switch (Meter_name(this)[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = cpus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (cpus + 1) / 2;
         break;
      case 'R': // Second Half
         *start = (cpus + 1) / 2;
         *count = cpus - *start;
         break;
   }
}

/* Maps the shown CPUs of this meter onto their sub-meters, creating the ones
 * seen for the first time. Sub-meters are kept per CPU id, so a CPU that is
 * hidden or shifts to another slot keeps its history. Returns true if the
 * shown sequence changed. */
static bool CPUMeter_commonMapCPUs(Meter* this) {
   const Machine* host = this->host;
   CPUMeterData* data = this->meterData;
   unsigned int start, count;
   AllCPUsMeter_getRange(this, &start, &count);

   if (host->existingCPUs != data->cpus) {
      for (unsigned int i = host->existingCPUs; i < data->cpus; i++)
         Meter_delete((Object*)data->meters[i]);

      if (host->existingCPUs > 0) {
         data->meters = xReallocArrayZero(data->meters, data->cpus, host->existingCPUs, sizeof(Meter*));
      } else {
         free(data->meters);
         data->meters = NULL;
      }
      data->cpus = host->existingCPUs;
   }

   bool changed = count != data->shownCount;
   if (changed) {
      if (count > 0) {
         data->shown = xReallocArray(data->shown, count, sizeof(Meter*));
      } else {
         free(data->shown);
         data->shown = NULL;
      }
      data->shownCount = count;
   }

   unsigned int slot = 0;
   for (unsigned int cpu = 0, seen = 0; cpu < data->cpus && slot < count; cpu++) {
      if (!CPUMeter_isShown(host, cpu))
         continue;
      if (seen++ < start)
         continue;

      if (!data->meters[cpu])
         data->meters[cpu] = Meter_new(host, cpu + 1, (const MeterClass*) Class(CPUMeter));

      if (!changed && data->shown[slot] != data->meters[cpu])
         changed = true;
      data->shown[slot++] = data->meters[cpu];
   }
   assert(slot == count);

   return changed;
}

static void CPUMeter_commonUpdateHeight(Meter* this) {
   const CPUMeterData* data = this->meterData;
   if (!data->shownCount) {
      this->h = 1;
      return;
   }
   int h = data->shown[0]->h;
   assert(h > 0);
   this->h = h * ((data->shownCount + data->ncol - 1) / data->ncol);
}

/* (Re)initializes every sub-meter, syncs it to this meter's mode and keeps
 * the height in sync with the shown CPUs (hot-plug, setup changes) */
static void CPUMeter_commonInitSubMeters(Meter* this) {
   const CPUMeterData* data = this->meterData;
   for (unsigned int i = 0; i < data->cpus; i++) {
      Meter* meter = data->meters[i];
      if (!meter)
         continue;

      Meter_init(meter);
      if (this->mode != 0)
         Meter_setMode(meter, this->mode);
   }

   CPUMeter_commonUpdateHeight(this);
}

static void CPUMeter_commonInit(Meter* this, unsigned int ncol) {
   CPUMeterData* data = this->meterData;
   if (!data) {
      data = xCalloc(1, sizeof(CPUMeterData));
      this->meterData = data;
   }
   data->ncol = ncol;

   CPUMeter_commonMapCPUs(this);
   CPUMeter_commonInitSubMeters(this);
}

static void SingleColCPUsMeter_init(Meter* this) {
   CPUMeter_commonInit(this, 1);
}

static void DualColCPUsMeter_init(Meter* this) {
   CPUMeter_commonInit(this, 2);
}

static void QuadColCPUsMeter_init(Meter* this) {
   CPUMeter_commonInit(this, 4);
}

static void OctoColCPUsMeter_init(Meter* this) {
   CPUMeter_commonInit(this, 8);
}

static void AllCPUsMeter_updateValues(Meter* this) {
   /* Reinit if the shown CPUs changed (e.g. hot-plug, hidden offline CPUs) */
   if (CPUMeter_commonMapCPUs(this))
      CPUMeter_commonInitSubMeters(this);

   const CPUMeterData* data = this->meterData;
   for (unsigned int i = 0; i < data->shownCount; i++)
      Meter_updateValues(data->shown[i]);
}

static void AllCPUsMeter_updateMode(Meter* this, MeterModeId mode) {
   this->mode = mode;
   Meter_init(this);
}

static void AllCPUsMeter_done(Meter* this) {
   CPUMeterData* data = this->meterData;
   for (unsigned int i = 0; i < data->cpus; i++)
      Meter_delete((Object*)data->meters[i]);
   free(data->meters);
   free(data->shown);
   free(data);
}

static void AllCPUsMeter_draw(Meter* this, int x, int y, int w) {
   const CPUMeterData* data = this->meterData;
   Meter** meters = data->shown;
   unsigned int count = data->shownCount;
   unsigned int ncol = data->ncol;
   int colwidth = w / (int)ncol;
   int diff = w % (int)ncol;
   unsigned int nrows = (count + ncol - 1) / ncol;
   for (unsigned int i = 0; i < count; i++) {
      unsigned int col = i / nrows;
      int d = (int)col > diff ? diff : (int)col; // dynamic spacer
      int xpos = x + ((int)col * colwidth) + d;
      int ypos = y + ((i % nrows) * meters[0]->h);
      meters[i]->draw(meters[i], xpos, ypos, colwidth);
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
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = CPU_METER_ITEMCOUNT,
   .isPercentChart = true,
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
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs",
   .uiName = "CPUs (1/1)",
   .description = "CPUs (1/1): all CPUs",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .description = "CPUs (1&2/2): all CPUs in 2 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .description = "CPUs (1/2): first half of list",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .description = "CPUs (2/2): second half of list",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = SingleColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .description = "CPUs (1&2/4): first half in 2 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .description = "CPUs (3&4/4): second half in 2 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = DualColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs4",
   .uiName = "CPUs (1&2&3&4/4)",
   .description = "CPUs (1&2&3&4/4): all CPUs in 4 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs4",
   .uiName = "CPUs (1-4/8)",
   .description = "CPUs (1-4/8): first half in 4 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs4",
   .uiName = "CPUs (5-8/8)",
   .description = "CPUs (5-8/8): second half in 4 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = QuadColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass AllCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "AllCPUs8",
   .uiName = "CPUs (1-8/8)",
   .description = "CPUs (1-8/8): all CPUs in 8 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass LeftCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "LeftCPUs8",
   .uiName = "CPUs (1-8/16)",
   .description = "CPUs (1-8/16): first half in 8 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

const MeterClass RightCPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = AllCPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = CPUMeter_attributes,
   .name = "RightCPUs8",
   .uiName = "CPUs (9-16/16)",
   .description = "CPUs (9-16/16): second half in 8 shorter columns",
   .caption = "CPU",
   .draw = AllCPUsMeter_draw,
   .init = OctoColCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};
