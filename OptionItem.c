/*
htop - OptionItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "OptionItem.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "CRT.h"
#include "Macros.h"
#include "RichString.h"
#include "XUtils.h"


static void OptionItem_delete(Object* cast) {
   OptionItem* this = (OptionItem*)cast;
   assert (this != NULL);

   free(this->text);
   free(this);
}

static void TextItem_display(const Object* cast, RichString* out) {
   const TextItem* this = (const TextItem*)cast;
   assert (this != NULL);

   RichString_appendWide(out, CRT_colors[HELP_BOLD], this->super.text);
}

static void CheckItem_display(const Object* cast, RichString* out) {
   const CheckItem* this = (const CheckItem*)cast;
   assert (this != NULL);

   RichString_writeAscii(out, CRT_colors[CHECK_BOX], "[");
   if (CheckItem_get(this)) {
      RichString_appendAscii(out, CRT_colors[CHECK_MARK], "x");
   } else {
      RichString_appendAscii(out, CRT_colors[CHECK_MARK], " ");
   }
   RichString_appendAscii(out, CRT_colors[CHECK_BOX], "]    ");
   RichString_appendWide(out, CRT_colors[CHECK_TEXT], this->super.text);
}

static void NumberItem_display(const Object* cast, RichString* out) {
   const NumberItem* this = (const NumberItem*)cast;
   assert (this != NULL);

   char buffer[12];
   RichString_writeAscii(out, CRT_colors[CHECK_BOX], "[");
   int written;
   if (this->scale < 0) {
      written = xSnprintf(buffer, sizeof(buffer), "%.*f", -this->scale, pow(10, this->scale) * NumberItem_get(this));
   } else if (this->scale > 0) {
      written = xSnprintf(buffer, sizeof(buffer), "%d", (int) (pow(10, this->scale) * NumberItem_get(this)));
   } else {
      written = xSnprintf(buffer, sizeof(buffer), "%d", NumberItem_get(this));
   }
   RichString_appendnAscii(out, CRT_colors[CHECK_MARK], buffer, written);
   RichString_appendAscii(out, CRT_colors[CHECK_BOX], "]");
   for (int i = written; i < 5; i++) {
      RichString_appendAscii(out, CRT_colors[CHECK_BOX], " ");
   }
   RichString_appendWide(out, CRT_colors[CHECK_TEXT], this->super.text);
}

const OptionItemClass OptionItem_class = {
   .super = {
      .extends = Class(Object),
      .delete = OptionItem_delete
   }
};

const OptionItemClass TextItem_class = {
   .super = {
      .extends = Class(OptionItem),
      .delete = OptionItem_delete,
      .display = TextItem_display
   },
   .kind = OPTION_ITEM_TEXT
};


const OptionItemClass CheckItem_class = {
   .super = {
      .extends = Class(OptionItem),
      .delete = OptionItem_delete,
      .display = CheckItem_display
   },
   .kind = OPTION_ITEM_CHECK
};


const OptionItemClass NumberItem_class = {
   .super = {
      .extends = Class(OptionItem),
      .delete = OptionItem_delete,
      .display = NumberItem_display
   },
   .kind = OPTION_ITEM_NUMBER
};

TextItem* TextItem_new(const char* text) {
   TextItem* this = AllocThis(TextItem);
   this->super.text = xStrdup(text);
   return this;
}

CheckItem* CheckItem_newByRef(const char* text, bool* ref) {
   CheckItem* this = AllocThis(CheckItem);
   this->super.text = xStrdup(text);
   this->value = false;
   this->ref = ref;
   return this;
}

CheckItem* CheckItem_newByVal(const char* text, bool value) {
   CheckItem* this = AllocThis(CheckItem);
   this->super.text = xStrdup(text);
   this->value = value;
   this->ref = NULL;
   return this;
}

bool CheckItem_get(const CheckItem* this) {
   if (this->ref) {
      return *(this->ref);
   } else {
      return this->value;
   }
}

void CheckItem_set(CheckItem* this, bool value) {
   if (this->ref) {
      *(this->ref) = value;
   } else {
      this->value = value;
   }
}

void CheckItem_toggle(CheckItem* this) {
   if (this->ref) {
      *(this->ref) = !*(this->ref);
   } else {
      this->value = !this->value;
   }
}

NumberItem* NumberItem_newByRef(const char* text, int* ref, int scale, int min, int max) {
   assert(min <= max);

   NumberItem* this = AllocThis(NumberItem);
   this->super.text = xStrdup(text);
   this->value = 0;
   this->ref = ref;
   this->scale = scale;
   this->min = min;
   this->max = max;
   return this;
}

NumberItem* NumberItem_newByVal(const char* text, int value, int scale, int min, int max) {
   assert(min <= max);

   NumberItem* this = AllocThis(NumberItem);
   this->super.text = xStrdup(text);
   this->value = CLAMP(value, min, max);
   this->ref = NULL;
   this->scale = scale;
   this->min = min;
   this->max = max;
   return this;
}

int NumberItem_get(const NumberItem* this) {
   if (this->ref) {
      return *(this->ref);
   } else {
      return this->value;
   }
}

void NumberItem_decrease(NumberItem* this) {
   if (this->ref) {
      *(this->ref) = CLAMP(*(this->ref) - 1, this->min, this->max);
   } else {
      this->value = CLAMP(this->value - 1, this->min, this->max);
   }
}

void NumberItem_increase(NumberItem* this) {
   if (this->ref) {
      *(this->ref) = CLAMP(*(this->ref) + 1, this->min, this->max);
   } else {
      this->value = CLAMP(this->value + 1, this->min, this->max);
   }
}

void NumberItem_toggle(NumberItem* this) {
   if (this->ref) {
      if (*(this->ref) >= this->max)
         *(this->ref) = this->min;
      else
         *(this->ref) += 1;
   } else {
      if (this->value >= this->max)
         this->value = this->min;
      else
         this->value += 1;
   }
}
