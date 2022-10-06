/*
htop - RNGEntropy.c
(C) 2022 Loic Jourdheuil
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/RNGEntropy.h"

#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "CRT.h"
#include "Object.h"
#include "XUtils.h"

#define INVALID_VALUE ((unsigned int)-1)

static size_t entropy = INVALID_VALUE;
static size_t poolsize = INVALID_VALUE;

static void RNGEntropy_updateValues(Meter* this) {
   char buffer[33];
   ssize_t r;

   if (poolsize == INVALID_VALUE) {
      r = xReadfile(PROCDIR "/sys/kernel/random/poolsize", buffer, sizeof(buffer));
      poolsize = r > 0 ? strtoul(buffer, NULL, 10) : 0;
   }

   r = xReadfile(PROCDIR "/sys/kernel/random/entropy_avail", buffer, sizeof(buffer));
   entropy = r > 0 ? strtoul(buffer, NULL, 10) : INVALID_VALUE;

   if (poolsize && poolsize != INVALID_VALUE && poolsize > this->total) {
      this->total = poolsize;
   }

   this->values[0] = poolsize ? entropy : NAN;
   if (entropy != INVALID_VALUE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%zu/%zu", entropy, poolsize);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "N/A");
      this->values[0] = NAN;
   }
}

static void RNGEntropy_display(ATTR_UNUSED const Object* cast, RichString* out) {
   char buffer[50];
   int len;

   if ((poolsize == INVALID_VALUE) || (entropy == INVALID_VALUE)) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
   } else {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Entropy: ");
      len = xSnprintf(buffer, sizeof(buffer), "%zu", entropy);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_OK], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " Poolsize: ");
      len = xSnprintf(buffer, sizeof(buffer), "%zu", poolsize);
      RichString_appendnAscii(out, CRT_colors[poolsize ? METER_VALUE_OK : METER_VALUE_ERROR], buffer, len);
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
   .total = 1.0,
   .attributes = RNGEntropy_attributes,
   .name = "Entropy",
   .uiName = "RNG Entropy",
   .description = "RNG entropy overview",
   .caption = "RNG"
};
