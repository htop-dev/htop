#ifndef HEADER_ListItem
#define HEADER_ListItem
/*
htop - ListItem.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Object.h"


typedef struct ListItem_ {
   Object super;
   char* value;
   int key;
   bool moving;
} ListItem;

extern const ObjectClass ListItem_class;

ListItem* ListItem_new(const char* value, int key);

void ListItem_append(ListItem* this, const char* text);

static inline const char* ListItem_getRef(const ListItem* this) {
   return this->value;
}

#endif
