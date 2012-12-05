/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableColumnsPanel.h"

#include "Header.h"
#include "ColumnsPanel.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct AvailableColumnsPanel_ {
   Panel super;
   Panel* columns;

   Settings* settings;
   ScreenManager* scr;
} AvailableColumnsPanel;

}*/

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   char* text = ((ListItem*) Panel_getSelected(super))->value;
   HandlerResult result = IGNORED;

   switch(ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
      {
         int at = Panel_getSelectedIndex(this->columns);
         Panel_insert(this->columns, at, (Object*) ListItem_new(text, 0));
         Panel_setSelected(this->columns, at+1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      }
      default:
      {
         if (isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         break;
      }
   }
   return result;
}

PanelClass AvailableColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableColumnsPanel_delete
   },
   .eventHandler = AvailableColumnsPanel_eventHandler
};

AvailableColumnsPanel* AvailableColumnsPanel_new(Settings* settings, Panel* columns, ScreenManager* scr) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true);
   
   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Available Columns");

   for (int i = 1; i < LAST_PROCESSFIELD; i++) {
      if (i != COMM)
         Panel_add(super, (Object*) ListItem_new(Process_fieldNames[i], 0));
   }
   this->columns = columns;
   return this;
}
