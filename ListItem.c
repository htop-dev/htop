/*
htop - ListItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
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


static void ListItem_delete(Object* cast) {
   ListItem* this = (ListItem*)cast;
   free(this->value);
   free(this);
}

static void ListItem_display(const Object* cast, RichString* out) {
   const ListItem* const this = (const ListItem*)cast;
   assert (this != NULL);
   /*
   int len = strlen(this->value)+1;
   char buffer[len+1];
   xSnprintf(buffer, len, "%s", this->value);
   */
   if (this->moving) {
      RichString_write(out, CRT_colors[DEFAULT_COLOR],
#ifdef HAVE_LIBNCURSESW
            CRT_utf8 ? "↕ " :
#endif
            "+ ");
   } else {
      RichString_prune(out);
   }
   RichString_append(out, CRT_colors[DEFAULT_COLOR], this->value/*buffer*/);
}

const ObjectClass ListItem_class = {
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};

ListItem* ListItem_new(const char* value, int key) {
   ListItem* this = AllocThis(ListItem);
   this->value = xStrdup(value);
   this->key = key;
   this->moving = false;
   return this;
}

void ListItem_append(ListItem* this, const char* text) {
   int oldLen = strlen(this->value);
   int textLen = strlen(text);
   int newLen = strlen(this->value) + textLen;
   this->value = xRealloc(this->value, newLen + 1);
   memcpy(this->value + oldLen, text, textLen);
   this->value[newLen] = '\0';
}

const char* ListItem_getRef(ListItem* this) {
   return this->value;
}

long ListItem_compare(const void* cast1, const void* cast2) {
   const ListItem* obj1 = (const ListItem*) cast1;
   const ListItem* obj2 = (const ListItem*) cast2;
   return strcmp(obj1->value, obj2->value);
}
