/*
htop - FileDescriptorMeter.c
(C) 2022 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FileDescriptorMeter.h"

#include <math.h>

#include "CRT.h"
#include "Macros.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


#define FD_EFFECTIVE_UNLIMITED(x) (!isgreaterequal((double)(1<<30), (x)))

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

   /* Use maximum value for scaling of bar mode
    *
    * As the plain total value can be very large compared to
    * the actually used value, this is capped in the following way:
    *
    * 1. If the maximum value is below (or equal to) 1<<16, use it directly
    * 2. If the maximum value is above, use powers of 2 starting at 1<<16 and
    *    double it until it's larger than 16 times the used file handles
    *    (capped at the maximum number of files)
    * 3. If the maximum is effectively unlimited (AKA > 1<<30),
    *    Do the same as for 2, but cap at 1<<30.
    */
   if (this->values[1] <= 1 << 16) {
      this->total = this->values[1];
   } else {
      if (this->total < 16 * this->values[0]) {
         for (this->total = 1 << 16; this->total < 16 * this->values[0]; this->total *= 2) {
            if (this->total >= 1 << 30) {
               break;
            }
         }
      }

      if (this->total > this->values[1]) {
         this->total = this->values[1];
      }

      if (this->total > 1 << 30) {
         this->total = 1 << 30;
      }
   }

   if (!isNonnegative(this->values[0])) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "unknown/unknown");
   } else if (FD_EFFECTIVE_UNLIMITED(this->values[1])) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.0lf/unlimited", this->values[0]);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.0lf/%.0lf", this->values[0], this->values[1]);
   }
}

static void FileDescriptorMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[50];
   int len;

   if (!isNonnegative(this->values[0])) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "unknown");
      return;
   }

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "used: ");
   len = xSnprintf(buffer, sizeof(buffer), "%.0lf", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[FILE_DESCRIPTOR_USED], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " max: ");
   if (FD_EFFECTIVE_UNLIMITED(this->values[1])) {
      RichString_appendAscii(out, CRT_colors[FILE_DESCRIPTOR_MAX], "unlimited");
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%.0lf", this->values[1]);
      RichString_appendnAscii(out, CRT_colors[FILE_DESCRIPTOR_MAX], buffer, len);
   }
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
   .total = 65536.0,
   .attributes = FileDescriptorMeter_attributes,
   .name = "FileDescriptors",
   .uiName = "File Descriptors",
   .caption = "FDs: ",
   .description = "Number of allocated/available file descriptors"
};
