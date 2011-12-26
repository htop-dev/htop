/*
htop - CheckItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CheckItem.h"

#include "CRT.h"

#include <assert.h>
#include <stdlib.h>

/*{
#include "Object.h"

typedef struct CheckItem_ {
   Object super;
   char* text;
   bool value;
   bool* ref;
} CheckItem;

}*/

#ifdef DEBUG
char* CHECKITEM_CLASS = "CheckItem";
#else
#define CHECKITEM_CLASS NULL
#endif

static void CheckItem_delete(Object* cast) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);

   free(this->text);
   free(this);
}

static void CheckItem_display(Object* cast, RichString* out) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);
   RichString_write(out, CRT_colors[CHECK_BOX], "[");
   if (CheckItem_get(this))
      RichString_append(out, CRT_colors[CHECK_MARK], "x");
   else
      RichString_append(out, CRT_colors[CHECK_MARK], " ");
   RichString_append(out, CRT_colors[CHECK_BOX], "] ");
   RichString_append(out, CRT_colors[CHECK_TEXT], this->text);
}

CheckItem* CheckItem_new(char* text, bool* ref, bool value) {
   CheckItem* this = malloc(sizeof(CheckItem));
   Object_setClass(this, CHECKITEM_CLASS);
   ((Object*)this)->display = CheckItem_display;
   ((Object*)this)->delete = CheckItem_delete;
   this->text = text;
   this->value = value;
   this->ref = ref;
   return this;
}

void CheckItem_set(CheckItem* this, bool value) {
   if (this->ref) 
      *(this->ref) = value;
   else
      this->value = value;
}

bool CheckItem_get(CheckItem* this) {
   if (this->ref) 
      return *(this->ref);
   else
      return this->value;
}
