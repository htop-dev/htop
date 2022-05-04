#ifndef HEADER_OptionItem
#define HEADER_OptionItem
/*
htop - OptionItem.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Object.h"


enum OptionItemType {
   OPTION_ITEM_TEXT,
   OPTION_ITEM_CHECK,
   OPTION_ITEM_NUMBER,
};

typedef struct OptionItemClass_ {
   const ObjectClass super;

   enum OptionItemType kind;
} OptionItemClass;

#define As_OptionItem(this_)                ((const OptionItemClass*)((this_)->super.klass))
#define OptionItem_kind(this_)              As_OptionItem(this_)->kind

typedef struct OptionItem_ {
   Object super;

   char* text;
} OptionItem;

typedef struct TextItem_ {
   OptionItem super;

   char* text;
} TextItem;

typedef struct CheckItem_ {
   OptionItem super;

   bool* ref;
   bool value;
} CheckItem;

typedef struct NumberItem_ {
   OptionItem super;

   char* text;
   int* ref;
   int value;
   int scale;
   int min;
   int max;
} NumberItem;

extern const OptionItemClass OptionItem_class;
extern const OptionItemClass TextItem_class;
extern const OptionItemClass CheckItem_class;
extern const OptionItemClass NumberItem_class;

TextItem* TextItem_new(const char* text);

CheckItem* CheckItem_newByRef(const char* text, bool* ref);
CheckItem* CheckItem_newByVal(const char* text, bool value);
bool CheckItem_get(const CheckItem* this);
void CheckItem_set(CheckItem* this, bool value);
void CheckItem_toggle(CheckItem* this);

NumberItem* NumberItem_newByRef(const char* text, int* ref, int scale, int min, int max);
NumberItem* NumberItem_newByVal(const char* text, int value, int scale, int min, int max);
int NumberItem_get(const NumberItem* this);
void NumberItem_decrease(NumberItem* this);
void NumberItem_increase(NumberItem* this);
void NumberItem_toggle(NumberItem* this);

#endif
