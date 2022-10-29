/*
htop - DynamicScreen.c
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "DynamicScreen.h"

#include <stdbool.h>
#include <stddef.h>

#include "Hashtable.h"
#include "Platform.h"
#include "XUtils.h"


Hashtable* DynamicScreens_new(Settings* settings) {
   return Platform_dynamicScreens(settings);
}

void DynamicScreens_delete(Hashtable* dynamics) {
   if (dynamics) {
      Platform_dynamicScreensDone(dynamics);
      Hashtable_delete(dynamics);
   }
}

typedef struct {
   ht_key_t key;
   const char* name;
   bool found;
} DynamicIterator;

static void DynamicScreen_compare(ht_key_t key, void* value, void* data) {
   const DynamicScreen* screen = (const DynamicScreen*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   if (String_eq(iter->name, screen->name)) {
      iter->found = true;
      iter->key = key;
   }
}

bool DynamicScreen_search(Hashtable* dynamics, const char* name, ht_key_t* key) {
   DynamicIterator iter = { .key = 0, .name = name, .found = false };
   if (dynamics)
      Hashtable_foreach(dynamics, DynamicScreen_compare, &iter);
   if (key)
      *key = iter.key;
   return iter.found;
}

const char* DynamicScreen_lookup(Hashtable* dynamics, ht_key_t key) {
   const DynamicScreen* screen = Hashtable_get(dynamics, key);
   return screen ? screen->name : NULL;
}

void DynamicScreen_availableColumns(char* currentScreen) {
   Platform_dynamicScreenAvailableColumns(currentScreen);
}
