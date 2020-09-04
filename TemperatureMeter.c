/*
htop - TemperatureMeter.c
(C) 2013 Ralf Stemmer
(C) 2014 Blair Bonnett
(C) 2020 Maxim Kurnosenko
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TemperatureMeter.h"

#include "ProcessList.h"
#include "CRT.h"
#include "StringUtils.h"
#include "XAlloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sensors/sensors.h>

typedef struct
{
   int size;
   int* current_temp;
   int* max_temp;
} Dynarr;

static int temp_converter(const Meter* this, double temp) {
   if (this->pl->settings->fahrenheitTemperature)
      return temp * 9 / 5 + 32;
   else
      return temp;
}

static Dynarr* get_cpu_temp(const Meter* this) {
   int core = 0, n = 0, count = 0;
   Dynarr* result = xMalloc(sizeof(Dynarr));

   for (const sensors_chip_name *chip = sensors_get_detected_chips(NULL, &n); chip; chip = sensors_get_detected_chips(NULL, &n)) {
      char buffer[64];
      if (sensors_snprintf_chip_name(buffer, sizeof(buffer), chip) < 0)
         continue;

      // cpu_thermal is for Raspberry Pi sensor support
      if (!String_startsWith(buffer, "coretemp") && !String_startsWith(buffer, "cpu_thermal"))
         continue;

      int m = 0;
      for (const sensors_feature *feature = sensors_get_features(chip, &m); feature; feature = sensors_get_features(chip, &m)) {
         if (feature->type == SENSORS_FEATURE_TEMP)
            count++;
      }

      result->current_temp = xCalloc(count, sizeof(int));
      result->max_temp = xCalloc(count, sizeof(int));
      result->size = count;

      m = 0;
      for (const sensors_feature *feature = sensors_get_features(chip, &m); feature; feature = sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         double local_temp;
         const sensors_subfeature *sub_feature;

         sub_feature = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (sub_feature) {
            if (sensors_get_value(chip, sub_feature->number, &local_temp) == 0)
               result->current_temp[core] = temp_converter(this, local_temp);
         }

         sub_feature = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_MAX);
         if (sub_feature) {
            if (sensors_get_value(chip, sub_feature->number, &local_temp) == 0)
               result->max_temp[core] = temp_converter(this, local_temp);
         }
         ++core;
      }
   }

   return result;
}

int TemperatureMeter_attributes[] = {
   TEMPERATURE_COOL,
   TEMPERATURE_MEDIUM,
   TEMPERATURE_HOT,
};

// Shows only package temperature in bars meter mode
static void TemperatureMeter_updateValues(Meter* this, char* buffer, int len) {
   Dynarr* core_temperature = get_cpu_temp(this);
   double temperature = core_temperature->current_temp[0];

   if (temperature < 0) {
      xSnprintf(buffer, len, "N/A");
      free(core_temperature);
      return;
   }

   this->total = core_temperature->max_temp[0];
   this->values[0] = temperature;

   if (this->pl->settings->fahrenheitTemperature)
      xSnprintf(buffer, len, "%d%sF", (int) temperature, CRT_degreeSign());
   else
      xSnprintf(buffer, len, "%d%sC", (int) temperature, CRT_degreeSign());
   free(core_temperature);
}

// Shows every physical core temperature in text meter mode
static void TemperatureMeter_display(const Object* cast, RichString* out) {
   const Meter* this          = (const Meter*) cast;
   Dynarr* cores_temperatures = get_cpu_temp(this);
   int temp_medium            = temp_converter(this, 55);
   int temp_hot               = temp_converter(this, 75);

   if (cores_temperatures->current_temp[0] < 0 || cores_temperatures->size == 0) {
      RichString_append(out, CRT_colors[METER_VALUE], "n/a");
      free(cores_temperatures);
      return;
   }

   int core_count_begin = cores_temperatures->size == 1 ? 0 : 1;

   for (int core = core_count_begin; core < cores_temperatures->size; ++core) {
      // convert double to integer
      int temperature = (int) cores_temperatures->current_temp[core];

      // choose the color for the temperature
      int tempColor;
      if (temperature < temp_medium)
         tempColor = CRT_colors[TEMPERATURE_COOL];
      else if (temperature < temp_hot)
         tempColor = CRT_colors[TEMPERATURE_MEDIUM];
      else
         tempColor = CRT_colors[TEMPERATURE_HOT];

      // output the temperature
      char buffer[4];
      snprintf(buffer, 4, "%3d", temperature);
      RichString_append(out, tempColor, buffer);
      snprintf(buffer, 3, "%s", CRT_degreeSign());
      RichString_append(out, CRT_colors[METER_TEXT], buffer);
      if (this->pl->settings->fahrenheitTemperature)
         RichString_append(out, CRT_colors[METER_TEXT], "F ");
      else
         RichString_append(out, CRT_colors[METER_TEXT], "C ");
   }
   free(cores_temperatures);
}

MeterClass TemperatureMeter_class = {
   .super = {
      .extends = Class(Meter),
      .display = TemperatureMeter_display,
      .delete = Meter_delete,
   },
   .updateValues = TemperatureMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = TemperatureMeter_attributes,
   .name = "CPUTemperature",
   .uiName = "CPU Temperature",
   .caption = "CPU Temperature: "
};
