/*
htop - CheckItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "CheckItem.h"

#include <assert.h>
#include <stdlib.h>

#include "CRT.h"
#include "RichString.h"


static void CheckItem_delete(Object* cast) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);

   free(this->text);
   free(this);
}

static void CheckItem_display(const Object* cast, RichString* out) {
   const CheckItem* this = (const CheckItem*)cast;
   assert (this != NULL);
   RichString_write(out, CRT_colors[CHECK_BOX], "[");
   if (CheckItem_get(this))
      RichString_append(out, CRT_colors[CHECK_MARK], "x");
   else
      RichString_append(out, CRT_colors[CHECK_MARK], " ");
   RichString_append(out, CRT_colors[CHECK_BOX], "] ");
   RichString_append(out, CRT_colors[CHECK_TEXT], this->text);
}

const ObjectClass CheckItem_class = {
   .display = CheckItem_display,
   .delete = CheckItem_delete
};

CheckItem* CheckItem_newByRef(char* text, bool* ref) {
   CheckItem* this = AllocThis(CheckItem);
   this->text = text;
   this->value = false;
   this->ref = ref;
   return this;
}

CheckItem* CheckItem_newByVal(char* text, bool value) {
   CheckItem* this = AllocThis(CheckItem);
   this->text = text;
   this->value = value;
   this->ref = NULL;
   return this;
}

void CheckItem_set(CheckItem* this, bool value) {
   if (this->ref)
      *(this->ref) = value;
   else
      this->value = value;
}

bool CheckItem_get(const CheckItem* this) {
   if (this->ref)
      return *(this->ref);
   else
      return this->value;
}
