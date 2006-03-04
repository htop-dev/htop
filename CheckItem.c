/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CheckItem.h"
#include "Object.h"
#include "CRT.h"

#include "debug.h"

/*{

typedef struct CheckItem_ {
   Object super;
   char* text;
   bool* value;
} CheckItem;

extern char* CHECKITEM_CLASS;
}*/

/* private property */
char* CHECKITEM_CLASS = "CheckItem";

CheckItem* CheckItem_new(char* text, bool* value) {
   CheckItem* this = malloc(sizeof(CheckItem));
   ((Object*)this)->class = CHECKITEM_CLASS;
   ((Object*)this)->display = CheckItem_display;
   ((Object*)this)->delete = CheckItem_delete;
   this->text = text;
   this->value = value;
   return this;
}

void CheckItem_delete(Object* cast) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);

   free(this->text);
   free(this);
}

void CheckItem_display(Object* cast, RichString* out) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);
   RichString_write(out, CRT_colors[CHECK_BOX], "[");
   if (*(this->value))
      RichString_append(out, CRT_colors[CHECK_MARK], "x");
   else
      RichString_append(out, CRT_colors[CHECK_MARK], " ");
   RichString_append(out, CRT_colors[CHECK_BOX], "] ");
   RichString_append(out, CRT_colors[CHECK_TEXT], this->text);
}
