/*
htop - DynamicColumn.c
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h" // IWYU pragma: keep

#include "DynamicColumn.h"

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "XUtils.h"


Hashtable* DynamicColumns_new(void) {
   return Platform_dynamicColumns();
}

typedef struct {
   unsigned int key;
   const char* name;
} DynamicIterator;

static void DynamicColumn_compare(ht_key_t key, void* value, void* data) {
   const DynamicColumn* column = (const DynamicColumn*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   if (String_eq(iter->name, column->name))
      iter->key = key;
}

unsigned int DynamicColumn_search(const ProcessList* pl, const char* name) {
   DynamicIterator iter = { .key = 0, .name = name };
   if (pl->dynamicColumns)
      Hashtable_foreach(pl->dynamicColumns, DynamicColumn_compare, &iter);
   return iter.key;
}

const char* DynamicColumn_lookup(const ProcessList* pl, unsigned int key) {
   const DynamicColumn* column = Hashtable_get(pl->dynamicColumns, key);
   return column ? column->name : NULL;
}

void DynamicColumn_writeField(const Process* proc, RichString* str, int field) {
   Platform_dynamicColumnWriteField(proc, str, field);
}
