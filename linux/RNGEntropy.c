/*
htop - RNGEntropy.c
(C) 2022 Loic Jourdheuil
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/RNGEntropy.h"

#include <stdlib.h>

#include "CRT.h"
#include "Object.h"
#include "XUtils.h"

#define INVALID_VALUE ((unsigned int)-1)

static unsigned int entropy = INVALID_VALUE;
static unsigned int poolsize = INVALID_VALUE;

static void RNGEntropy_updateValues(Meter* this) {
   char buffer[11];
   ssize_t r;

   if (poolsize == INVALID_VALUE) {
      r = xReadfile(PROCDIR "/sys/kernel/random/poolsize", buffer, sizeof(buffer));
      if (r <= 0) {
         goto err;
      }
      poolsize = (unsigned int)strtoul(buffer, NULL, 10);
      if (poolsize == 0) {
         /* invalid poolsize value */
         poolsize = INVALID_VALUE;
         goto err;
      }
   }
   r = xReadfile(PROCDIR "/sys/kernel/random/entropy_avail", buffer, sizeof(buffer));
   if (r <= 0) {
      entropy = INVALID_VALUE;
      goto err;
   } else {
      entropy = (unsigned int)strtoul(buffer, NULL, 10);
   }

   this->values[0] = (100.0 * entropy) / poolsize;
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f%%", this->values[0]);
   return;

err:
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
   this->values[0] = 0.0;
}

static void RNGEntropy_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[50];
   int len;

   if ((poolsize == INVALID_VALUE) || (entropy == INVALID_VALUE)) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE_OK], "N/A");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
   } else {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
      len = xSnprintf(buffer, sizeof(buffer), "%u", entropy);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_OK], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
      len = xSnprintf(buffer, sizeof(buffer), "%u", poolsize);
      RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
   }
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
