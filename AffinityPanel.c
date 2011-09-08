
#include "AffinityPanel.h"

#include "Panel.h"
#include "CheckItem.h"
#include "ProcessList.h"

#include "debug.h"
#include <assert.h>

static HandlerResult AffinityPanel_eventHandler(Panel* this, int ch) {
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) Panel_getSelected(this);
   switch(ch) {
   case KEY_MOUSE:
   case ' ':
      CheckItem_set(selected, ! (CheckItem_get(selected)) );
      result = HANDLED;
      break;
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
      result = BREAK_LOOP;
      break;
   }
   return result;
}

Panel* AffinityPanel_new(ProcessList* pl, unsigned long mask) {
   Panel* this = Panel_new(1, 1, 1, 1, CHECKITEM_CLASS, true, ListItem_compare);
   this->eventHandler = AffinityPanel_eventHandler;

   Panel_setHeader(this, "Use CPUs:");
   for (int i = 0; i < pl->cpuCount; i++) {
      char number[10];
      snprintf(number, 9, "%d", ProcessList_cpuId(pl, i));
      Panel_add(this, (Object*) CheckItem_new(String_copy(number), NULL, mask & (1 << i)));
   }
   return this;
}

unsigned long AffinityPanel_getAffinity(Panel* this) {
   int size = Panel_size(this);
   unsigned long mask = 0;
   for (int i = 0; i < size; i++) {
      if (CheckItem_get((CheckItem*)Panel_get(this, i)))
         mask = mask | (1 << i);
   }
   return mask;
}
