/*
htop - AffinityPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AffinityPanel.h"

#include "CheckItem.h"

#include <assert.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Affinity.h"
#include "ProcessList.h"
#include "ListItem.h"
}*/

static HandlerResult AffinityPanel_eventHandler(Panel* this, int ch) {
   CheckItem* selected = (CheckItem*) Panel_getSelected(this);
   switch(ch) {
   case KEY_MOUSE:
   case ' ':
      CheckItem_set(selected, ! (CheckItem_get(selected)) );
      return HANDLED;
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
      return BREAK_LOOP;
   }
   return IGNORED;
}

Panel* AffinityPanel_new(ProcessList* pl, Affinity* affinity) {
   Panel* this = Panel_new(1, 1, 1, 1, CHECKITEM_CLASS, true, ListItem_compare);
   this->eventHandler = AffinityPanel_eventHandler;

   Panel_setHeader(this, "Use CPUs:");
   int curCpu = 0;
   for (int i = 0; i < pl->cpuCount; i++) {
      char number[10];
      snprintf(number, 9, "%d", ProcessList_cpuId(pl, i));
      bool mode;
      if (curCpu < affinity->used && affinity->cpus[curCpu] == i) {
         mode = true;
         curCpu++;
      } else {
         mode = false;
      }
      Panel_add(this, (Object*) CheckItem_new(strdup(number), NULL, mode));
   }
   return this;
}

Affinity* AffinityPanel_getAffinity(Panel* this) {
   Affinity* affinity = Affinity_new();
   int size = Panel_size(this);
   for (int i = 0; i < size; i++) {
      if (CheckItem_get((CheckItem*)Panel_get(this, i)))
         Affinity_add(affinity, i);
   }
   return affinity;
}
