/*
htop - ListItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ListItem.h"

#include "CRT.h"
#include "String.h"
#include "RichString.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

/*{
#include "Object.h"

typedef struct ListItem_ {
   Object super;
   char* value;
   int key;
} ListItem;

}*/

#ifdef DEBUG
char* LISTITEM_CLASS = "ListItem";
#else
#define LISTITEM_CLASS NULL
#endif

static void ListItem_delete(Object* cast) {
   ListItem* this = (ListItem*)cast;
   free(this->value);
   free(this);
}

static void ListItem_display(Object* cast, RichString* out) {
   ListItem* this = (ListItem*)cast;
   assert (this != NULL);
   int len = strlen(this->value)+1;
   char buffer[len+1];
   snprintf(buffer, len, "%s", this->value);
   RichString_write(out, CRT_colors[DEFAULT_COLOR], buffer);
}

ListItem* ListItem_new(const char* value, int key) {
   ListItem* this = malloc(sizeof(ListItem));
   Object_setClass(this, LISTITEM_CLASS);
   ((Object*)this)->display = ListItem_display;
   ((Object*)this)->delete = ListItem_delete;
   this->value = strdup(value);
   this->key = key;
   return this;
}

void ListItem_append(ListItem* this, char* text) {
   char* buf = malloc(strlen(this->value) + strlen(text) + 1);
   sprintf(buf, "%s%s", this->value, text);
   free(this->value);
   this->value = buf;
}

const char* ListItem_getRef(ListItem* this) {
   return this->value;
}

int ListItem_compare(const void* cast1, const void* cast2) {
   ListItem* obj1 = (ListItem*) cast1;
   ListItem* obj2 = (ListItem*) cast2;
   return strcmp(obj1->value, obj2->value);
}

