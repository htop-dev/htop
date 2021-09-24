/*
htop - DynamicColumn.c
(C) 2021 Sohaib Mohammed
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DynamicColumn.h"

#include <stddef.h>

#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


Hashtable* DynamicColumns_new(void) {
   return Platform_dynamicColumns();
}

void DynamicColumns_delete(Hashtable* dynamics) {
   if (dynamics) {
      Platform_dynamicColumnsDone(dynamics);
      Hashtable_delete(dynamics);
   }
}

const char* DynamicColumn_init(unsigned int key) {
   return Platform_dynamicColumnInit(key);
}

typedef struct {
   const char* name;
   const DynamicColumn* data;
   unsigned int key;
} DynamicIterator;

static void DynamicColumn_compare(ht_key_t key, void* value, void* data) {
   const DynamicColumn* column = (const DynamicColumn*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   if (String_eq(iter->name, column->name)) {
      iter->data = column;
      iter->key = key;
   }
}

const DynamicColumn* DynamicColumn_search(Hashtable* dynamics, const char* name, unsigned int* key) {
   DynamicIterator iter = { .key = 0, .data = NULL, .name = name };
   if (dynamics)
      Hashtable_foreach(dynamics, DynamicColumn_compare, &iter);
   if (key)
      *key = iter.key;
   return iter.data;
}

const DynamicColumn* DynamicColumn_lookup(Hashtable* dynamics, unsigned int key) {
   return (const DynamicColumn*) Hashtable_get(dynamics, key);
}

bool DynamicColumn_writeField(const Process* proc, RichString* str, unsigned int key) {
   return Platform_dynamicColumnWriteField(proc, str, key);
}
