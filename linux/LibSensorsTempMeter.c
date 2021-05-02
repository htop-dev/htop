/*
htop - LibSensorsTempMeter.c
(C) 2021 htop dev team
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LibSensorsTempMeter.h"

#ifdef HAVE_SENSORS_SENSORS_H

#include <math.h>

#include "CRT.h"
#include "linux/LibSensors.h"


static const int LibSensorsTempMeter_attributes[] = {
   METER_VALUE,
};

static void LibSensorsTempMeter_updateValues(Meter* this) {
   const Settings* settings = this->pl->settings;

   if (!this->curChoice) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      return;
   }

   LibSensors_getTemp(this->curChoice, &this->values[0], &this->total);

   if (isnan(this->values[0]))
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - N/A", this->curChoice);
   else if (settings->degreeFahrenheit)
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - %d%sF",
                this->curChoice,
                (int)(this->values[0] * 9 / 5 + 32),
                CRT_degreeSign);
   else
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s - %d%sC",
                this->curChoice,
                (int)this->values[0],
                CRT_degreeSign);

   /* Only fill bar if total is available */
   this->curItems = isnan(this->total) ? 0 : 1;
}

static void LibSensorsTempMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*) cast;
   const Settings* settings = this->pl->settings;

   if (!this->curChoice) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid choice");
      return;
   } else if (isnan(this->values[0])) {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "Invalid data for ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], this->curChoice);
      return;
   }

   char buffer[32], buffer2[16];
   int len, len2;

   len = RichString_appendAscii(out, CRT_colors[METER_VALUE], this->curChoice);
   if (len < 15)
      RichString_appendChr(out, CRT_colors[METER_TEXT], ' ', 15 - len);

   if (settings->degreeFahrenheit)
      len = xSnprintf(buffer, sizeof(buffer), " %d%sF",
                      (int)(this->values[0] * 9 / 5 + 32),
                      CRT_degreeSign);
   else
      len = xSnprintf(buffer, sizeof(buffer), " %d%sC",
                      (int)this->values[0],
                      CRT_degreeSign);
   RichString_appendnWide(out, CRT_colors[METER_VALUE], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "  max: ");

   if (isnan(this->total))
      len2 = xSnprintf(buffer2, sizeof(buffer2), "N/A");
   else if (settings->degreeFahrenheit)
      len2 = xSnprintf(buffer2, sizeof(buffer2), "%d%sF", (int)(this->total * 9 / 5 + 32), CRT_degreeSign);
   else
      len2 = xSnprintf(buffer2, sizeof(buffer2), "%d%sC", (int)this->total, CRT_degreeSign);
   RichString_appendnWide(out, CRT_colors[METER_VALUE], buffer2, len2);
}

const MeterClass LibSensorsTempMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = LibSensorsTempMeter_display
   },
   .updateValues = LibSensorsTempMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = LibSensorsTempMeter_attributes,
   .name = "SensorTemp",
   .uiName = "Sensor Temperature",
   .caption = "Temp: ",
   .getChoices = LibSensors_getTempChoices,
};

#endif /* HAVE_SENSORS_SENSORS_H */
