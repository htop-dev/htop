/*
htop - DynamicScreen.c
(C) 2022 Sohaib Mohammed
(C) 2022-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DynamicScreen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "Hashtable.h"
#include "Platform.h"
#include "XUtils.h"


Hashtable* DynamicScreens_new(void) {
   return Platform_dynamicScreens();
}

void DynamicScreens_delete(Hashtable* screens) {
   if (screens) {
      Platform_dynamicScreensDone(screens);
      Hashtable_delete(screens);
   }
}

void DynamicScreen_done(DynamicScreen* this) {
   free(this->caption);
   free(this->fields);
   free(this->heading);
   free(this->sortKey);
   free(this->columnKeys);
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

bool DynamicScreen_search(Hashtable* screens, const char* name, ht_key_t* key) {
   DynamicIterator iter = { .key = 0, .name = name, .found = false };
   if (screens)
      Hashtable_foreach(screens, DynamicScreen_compare, &iter);
   if (key)
      *key = iter.key;
   return iter.found;
}

const char* DynamicScreen_lookup(Hashtable* screens, ht_key_t key) {
   const DynamicScreen* screen = Hashtable_get(screens, key);
   return screen ? screen->name : NULL;
}
