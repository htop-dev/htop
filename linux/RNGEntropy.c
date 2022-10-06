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

static unsigned int entropy = 0;
static unsigned int poolsize = 0;

static void getRNGEntropy(void) {
   FILE* file = fopen(PROCDIR "/sys/kernel/random/entropy_avail", "r");
   if (!file)
      return;
   if (!fscanf(file, "%u", &entropy))
      entropy = 0;
   fclose(file);
}

static void getRNGPoolsize(void) {
   /* Read poolsize only the first time */
   if (poolsize > 0)
      return;

   FILE* file = fopen(PROCDIR "/sys/kernel/random/poolsize", "r");
   if (!file)
      return;
   if (!fscanf(file, "%u", &poolsize))
      poolsize = 0;
   fclose(file);
}

static void RNGEntropy_updateValues(Meter* this) {
   getRNGPoolsize();
   getRNGEntropy();

   if ((poolsize == 0) || (entropy == 0)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   }

   this->values[0] = (100.0 * entropy) / poolsize;
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", this->values[0]);
}

static void RNGEntropy_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[50];
   int len;

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
   len = xSnprintf(buffer, sizeof(buffer), "%u", entropy);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE_OK], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
   len = xSnprintf(buffer, sizeof(buffer), "%u", poolsize);
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
   .total = 100.0,
   .attributes = RNGEntropy_attributes,
   .name = "Entropy",
   .uiName = "RNG Entropy",
   .description = "RNG entropy overview",
   .caption = "RNG"
};