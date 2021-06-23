/*
htop - DynamicMeter.c
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h" // IWYU pragma: keep

#include "DynamicMeter.h"

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
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

typedef struct {
   unsigned int key;
   const char* name;
} DynamicIterator;

static void DynamicMeter_compare(ht_key_t key, void* value, void* data) {
   const DynamicMeter* meter = (const DynamicMeter*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   if (String_eq(iter->name, meter->name))
      iter->key = key;
}

unsigned int DynamicMeter_search(const ProcessList* pl, const char* name) {
   DynamicIterator iter = { .key = 0, .name = name };
   if (pl->dynamicMeters)
      Hashtable_foreach(pl->dynamicMeters, DynamicMeter_compare, &iter);
   return iter.key;
}

const char* DynamicMeter_lookup(const ProcessList* pl, unsigned int key) {
   const DynamicMeter* meter = Hashtable_get(pl->dynamicMeters, key);
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

static void DynamicMeter_getUiName(const Meter* this, char* name, size_t length) {
   const ProcessList* pl = this->pl;
   const DynamicMeter* meter = Hashtable_get(pl->dynamicMeters, this->param);
   if (meter) {
      const char* uiName = meter->caption ? meter->caption : meter->name;
      xSnprintf(name, length, "%s", uiName);
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
   .getUiName = DynamicMeter_getUiName,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = DynamicMeter_attributes,
   .name = "Dynamic",
   .uiName = "Dynamic",
   .caption = "",
};
