/*
htop - RNGEntropy.c
(C) 2022 Loic Jourdheuil
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/RNGEntropy.h"

#include "CRT.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#include "Object.h"
#include "XUtils.h"

static void getRNGEntropy(Meter* this) {
   FILE* file = fopen(PROCDIR "/sys/kernel/random/entropy_avail", "r");
   if (!file)
      return;
   int match = fscanf(file, "%le", &this->values[0]);
   (void) match;
   fclose(file);
}

static void getRNGPoolsize(Meter* this) {
   FILE* file = fopen(PROCDIR "/sys/kernel/random/poolsize", "r");
   if (!file)
      return;
   int match = fscanf(file, "%le", &this->total);
   (void) match;
   fclose(file);
}

static void RNGEntropy_updateValues(Meter* this) {
   getRNGEntropy(this);
   getRNGPoolsize(this);

   if ((this->total <= 0) || (this->values[0] <= 0)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", 100.0 * this->values[0] / this->total);
}

static void RNGEntropy_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[50];
   int len;

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
   len = xSnprintf(buffer, sizeof(buffer), "%.0f", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE_OK], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
   len = xSnprintf(buffer, sizeof(buffer), "%.0f", this->total);
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
}

static const int RNGEntropy_attributes[] = {
   METER_VALUE_OK,
};

const MeterClass RNGEntropy_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = RNGEntropy_display,
   },
   .updateValues = RNGEntropy_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 1,
   .total = 4096.0,
   .attributes = RNGEntropy_attributes,
   .name = "Entropy",
   .uiName = "RNG Entropy",
   .description = "RNG entropy overview",
   .caption = "RNG"
};