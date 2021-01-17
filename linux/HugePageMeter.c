/*
htop - HugePageMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "HugePageMeter.h"

#include "LinuxProcessList.h"

#include <limits.h>
#include <math.h>

#include "CRT.h"
#include "Object.h"
#include "RichString.h"

static const char *HugePageMeter_active_labels[4] = { NULL, NULL, NULL, NULL };

static const int HugePageMeter_attributes[] = {
   HUGEPAGE_1,
   HUGEPAGE_2,
   HUGEPAGE_3,
   HUGEPAGE_4,
};

static const char* const HugePageMeter_labels[] = {
   " 64K:", " 128K:", " 256K:", " 512K:",
   " 1M:", " 2M:", " 4M:", " 8M:", " 16M:", " 32M:", " 64M:", " 128M:", " 256M:", " 512M:",
   " 1G:", " 2G:", " 4G:", " 8G:", " 16G:", " 32G:", " 64G:", " 128G:", " 256G:", " 512G:",
};

static void HugePageMeter_updateValues(Meter* this, char* buffer, size_t size) {
   assert(ARRAYSIZE(HugePageMeter_labels) == HTOP_HUGEPAGE_COUNT);

   int written;
   unsigned long long int usedTotal = 0;
   unsigned nextUsed = 0;

   const LinuxProcessList* lpl = (const LinuxProcessList*) this->pl;
   this->total = lpl->totalHugePageMem;
   this->values[0] = 0;
   HugePageMeter_active_labels[0] = " used:";
   for (unsigned i = 1; i < ARRAYSIZE(HugePageMeter_active_labels); i++) {
      this->values[i] = NAN;
      HugePageMeter_active_labels[i] = NULL;
   }
   for (unsigned i = 0; i < HTOP_HUGEPAGE_COUNT; i++) {
      unsigned long long int value = lpl->usedHugePageMem[i];
      if (value != ULLONG_MAX) {
         this->values[nextUsed] = value;
         usedTotal += value;
         HugePageMeter_active_labels[nextUsed] = HugePageMeter_labels[i];
         if (++nextUsed == ARRAYSIZE(HugePageMeter_active_labels)) {
            break;
         }
      }
   }

   written = Meter_humanUnit(buffer, usedTotal, size);
   METER_BUFFER_CHECK(buffer, size, written);

   METER_BUFFER_APPEND_CHR(buffer, size, '/');

   Meter_humanUnit(buffer, this->total, size);
}

static void HugePageMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   const Meter* this = (const Meter*)cast;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   for (unsigned i = 0; i < ARRAYSIZE(HugePageMeter_active_labels); i++) {
      if (isnan(this->values[i])) {
         break;
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], HugePageMeter_active_labels[i]);
      Meter_humanUnit(buffer, this->values[i], sizeof(buffer));
      RichString_appendAscii(out, CRT_colors[HUGEPAGE_1 + i], buffer);
   }
}

const MeterClass HugePageMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = HugePageMeter_display,
   },
   .updateValues = HugePageMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = ARRAYSIZE(HugePageMeter_active_labels),
   .total = 100.0,
   .attributes = HugePageMeter_attributes,
   .name = "HugePages",
   .uiName = "HugePages",
   .caption = "HP"
};
