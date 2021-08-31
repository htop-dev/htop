/*
htop - ListItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ListItem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "RichString.h"
#include "XUtils.h"


void ListItem_delete(Object* cast) {
   ListItem* this = (ListItem*)cast;
   free(this->value);
   free(this);
}

void ListItem_display(const Object* cast, RichString* out) {
   const ListItem* const this = (const ListItem*)cast;
   assert (this != NULL);

   if (this->moving) {
      RichString_writeWide(out, CRT_colors[DEFAULT_COLOR],
#ifdef HAVE_LIBNCURSESW
                           CRT_utf8 ? "↕ " :
#endif
                           "+ ");
   }
   RichString_appendWide(out, CRT_colors[DEFAULT_COLOR], this->value);
}

void ListItem_init(ListItem* this, const char* value, int key) {
   this->value = xStrdup(value);
   this->key = key;
   this->moving = false;
}

ListItem* ListItem_new(const char* value, int key) {
   ListItem* this = AllocThis(ListItem);
   ListItem_init(this, value, key);
   return this;
}

void ListItem_append(ListItem* this, const char* text) {
   size_t oldLen = strlen(this->value);
   size_t textLen = strlen(text);
   size_t newLen = oldLen + textLen;
   this->value = xRealloc(this->value, newLen + 1);
   memcpy(this->value + oldLen, text, textLen);
   this->value[newLen] = '\0';
}

int ListItem_compare(const void* cast1, const void* cast2) {
   const ListItem* obj1 = (const ListItem*) cast1;
   const ListItem* obj2 = (const ListItem*) cast2;
   return strcmp(obj1->value, obj2->value);
}

const ObjectClass ListItem_class = {
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};
