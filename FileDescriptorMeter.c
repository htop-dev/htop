/*
htop - FileDescriptorMeter.c
(C) 2022 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "FileDescriptorMeter.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "CRT.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int FileDescriptorMeter_attributes[] = {
   FILE_DESCRIPTOR_USED,
   FILE_DESCRIPTOR_MAX
};

static void FileDescriptorMeter_updateValues(Meter* this) {
   this->values[0] = 0;
   this->values[1] = 1;

   Platform_getFileDescriptors(&this->values[0], &this->values[1]);

   /* only print bar for first value */
   this->curItems = 1;

   /* Use maximum value for scaling of bar mode */
   this->total = this->values[1];

   if (isnan(this->values[0])) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "unknown/unknown");
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.0lf/%.0lf", this->values[0], this->values[1]);
   }
}

static void FileDescriptorMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[50];
   int len;

   if (isnan(this->values[0])) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "unknown");
      return;
   }

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "used: ");
   len = xSnprintf(buffer, sizeof(buffer), "%.0lf", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[FILE_DESCRIPTOR_USED], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " max: ");
   len = xSnprintf(buffer, sizeof(buffer), "%.0lf", this->values[1]);
   RichString_appendnAscii(out, CRT_colors[FILE_DESCRIPTOR_MAX], buffer, len);
}

const MeterClass FileDescriptorMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = FileDescriptorMeter_display,
   },
   .updateValues = FileDescriptorMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 2,
   .total = 100.0,
   .attributes = FileDescriptorMeter_attributes,
   .name = "FileDescriptors",
   .uiName = "File Descriptors",
   .caption = "FDs: ",
   .description = "Number of allocated/available file descriptors"
};
