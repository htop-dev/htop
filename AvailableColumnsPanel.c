/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableColumnsPanel.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ColumnsPanel.h"
#include "FunctionBar.h"
#include "ListItem.h"
#include "Object.h"
#include "Platform.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static const char* const AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   HandlerResult result = IGNORED;

   switch(ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
      {
         const ListItem* selected = (ListItem*) Panel_getSelected(super);
         if (!selected)
            break;

         int key = selected->key;
         int at = Panel_getSelectedIndex(this->columns);
         Panel_insert(this->columns, at, (Object*) ListItem_new(Process_fields[key].name, key));
         Panel_setSelected(this->columns, at+1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      }
      default:
      {
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         break;
      }
   }
   return result;
}

const PanelClass AvailableColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableColumnsPanel_delete
   },
   .eventHandler = AvailableColumnsPanel_eventHandler
};

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(AvailableColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   Panel_setHeader(super, "Available Columns");

   for (int i = 1; i < LAST_PROCESSFIELD; i++) {
      if (i != COMM && Process_fields[i].description) {
         char description[256];
         xSnprintf(description, sizeof(description), "%s - %s", Process_fields[i].name, Process_fields[i].description);
         Panel_add(super, (Object*) ListItem_new(description, i));
      }
   }
   this->columns = columns;
   return this;
}
