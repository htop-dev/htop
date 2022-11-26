/*
htop - LibSensorsFanMeter.c
(C) 2021 htop dev team
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LibSensorsFanMeter.h"

#ifdef HAVE_SENSORS_SENSORS_H

#include <math.h>

#include "CRT.h"
#include "linux/LibSensors.h"


static const int LibSensorsFanMeter_attributes[] = {
   METER_VALUE,
};

static void LibSensorsFanMeter_updateValues(Meter* this) {
   if (!this->curChoice) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      return;
   }

   LibSensors_getFan(this->curChoice, &this->values[0], &this->values[1], &this->total);

   if (isnan(this->values[0]))
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - N/A", this->curChoice);
   else
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - %d",
                this->curChoice,
                (int)this->values[0]);

   /* Only fill bar with current value and only if total is available */
   this->curItems = isnan(this->total) ? 0 : 1;
}

static void LibSensorsFanMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*) cast;

   if (!this->curChoice) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid choice");
      return;
   } else if (isnan(this->values[0])) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid data for ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], this->curChoice);
      return;
   }

   char buffer[16];
   int len;

   len = RichString_appendAscii(out, CRT_colors[METER_VALUE], this->curChoice);
   if (len < 15)
      RichString_appendChr(out, CRT_colors[METER_TEXT], ' ', 15 - len);

   len = xSnprintf(buffer, sizeof(buffer), " %.0f", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "  min: ");

   if (isnan(this->values[1])) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%.0f", this->values[1]);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
   }

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "  max: ");

   if (isnan(this->total)) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%.0f", this->total);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
   }
}

const MeterClass LibSensorsFanMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LibSensorsFanMeter_display
   },
   .updateValues = LibSensorsFanMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 2,
   .total = 100.0,
   .attributes = LibSensorsFanMeter_attributes,
   .name = "SensorFan",
   .uiName = "Sensor Fan Speed",
   .caption = "Fan: ",
   .getChoices = LibSensors_getFanChoices,
};

#endif /* HAVE_SENSORS_SENSORS_H */
