/*
htop - DynamicMeter.c
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DynamicMeter.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "CRT.h"
#include "Machine.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static const int DynamicMeter_attributes[] = {
   DYNAMIC_GRAY,
   DYNAMIC_DARKGRAY,
   DYNAMIC_RED,
   DYNAMIC_GREEN,
   DYNAMIC_BLUE,
   DYNAMIC_CYAN,
   DYNAMIC_MAGENTA,
   DYNAMIC_YELLOW,
   DYNAMIC_WHITE
};

Hashtable* DynamicMeters_new(void) {
   return Platform_dynamicMeters();
}

void DynamicMeters_delete(Hashtable* dynamics) {
   if (dynamics) {
      Platform_dynamicMetersDone(dynamics);
      Hashtable_delete(dynamics);
   }
}

typedef struct {
   unsigned int key;
   const char* name;
   bool found;
} DynamicIterator;

static void DynamicMeter_compare(ht_key_t key, void* value, void* data) {
   const DynamicMeter* meter = (const DynamicMeter*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   if (String_eq(iter->name, meter->name)) {
      iter->found = true;
      iter->key = key;
   }
}

bool DynamicMeter_search(Hashtable* dynamics, const char* name, unsigned int* key) {
   DynamicIterator iter = { .key = 0, .name = name, .found = false };
   if (dynamics)
      Hashtable_foreach(dynamics, DynamicMeter_compare, &iter);
   if (key)
      *key = iter.key;
   return iter.found;
}

const char* DynamicMeter_lookup(Hashtable* dynamics, unsigned int key) {
   const DynamicMeter* meter = Hashtable_get(dynamics, key);
   return meter ? meter->name : NULL;
}

static void DynamicMeter_init(Meter* meter) {
   Platform_dynamicMeterInit(meter);
}

static void DynamicMeter_updateValues(Meter* meter) {
   Platform_dynamicMeterUpdateValues(meter);
}

static void DynamicMeter_display(const Object* cast, RichString* out) {
   const Meter* meter = (const Meter*)cast;
   Platform_dynamicMeterDisplay(meter, out);
}

static const char* DynamicMeter_getCaption(const Meter* this) {
   const Settings* settings = this->host->settings;
   const DynamicMeter* meter = Hashtable_get(settings->dynamicMeters, this->param);
   if (meter)
      return meter->caption ? meter->caption : meter->name;
   return this->caption;
}

static void DynamicMeter_getUiName(const Meter* this, char* name, size_t length) {
   assert(length > 0);

   const Settings* settings = this->host->settings;
   const DynamicMeter* meter = Hashtable_get(settings->dynamicMeters, this->param);
   if (meter) {
      const char* uiName = meter->caption;
      if (uiName) {
         size_t uiNameLen = strlen(uiName);
         if (uiNameLen > 2 && uiName[uiNameLen - 2] == ':')
            uiNameLen -= 2;

         String_safeStrncpy(name, uiName, MINIMUM(length, uiNameLen + 1));
      } else {
         String_safeStrncpy(name, meter->name, length);
      }
   }
}

const MeterClass DynamicMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = DynamicMeter_display
   },
   .init = DynamicMeter_init,
   .updateValues = DynamicMeter_updateValues,
   .getCaption = DynamicMeter_getCaption,
   .getUiName = DynamicMeter_getUiName,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 0,
   .total = 100.0,
   .attributes = DynamicMeter_attributes,
   .name = "Dynamic",
   .uiName = "Dynamic",
   .caption = "",
};
