
#include "AvailableColumnsListBox.h"
#include "Settings.h"
#include "Header.h"
#include "ScreenManager.h"
#include "ColumnsListBox.h"

#include "ListBox.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct AvailableColumnsListBox_ {
   ListBox super;
   ListBox* columns;

   Settings* settings;
   ScreenManager* scr;
} AvailableColumnsListBox;

}*/

AvailableColumnsListBox* AvailableColumnsListBox_new(Settings* settings, ListBox* columns, ScreenManager* scr) {
   AvailableColumnsListBox* this = (AvailableColumnsListBox*) malloc(sizeof(AvailableColumnsListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = AvailableColumnsListBox_delete;
   
   this->settings = settings;
   this->scr = scr;
   super->eventHandler = AvailableColumnsListBox_eventHandler;

   ListBox_setHeader(super, "Available Columns");

   for (int i = 1; i < LAST_PROCESSFIELD; i++) {
      if (i != COMM)
         ListBox_add(super, (Object*) ListItem_new(Process_fieldNames[i], 0));
   }
   this->columns = columns;
   return this;
}

void AvailableColumnsListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   AvailableColumnsListBox* this = (AvailableColumnsListBox*) object;
   ListBox_done(super);
   free(this);
}

HandlerResult AvailableColumnsListBox_eventHandler(ListBox* super, int ch) {
   AvailableColumnsListBox* this = (AvailableColumnsListBox*) super;
   char* text = ((ListItem*) ListBox_getSelected(super))->value;
   HandlerResult result = IGNORED;

   switch(ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
      {
         int at = ListBox_getSelectedIndex(this->columns) + 1;
         if (at == ListBox_getSize(this->columns))
            at--;
         ListBox_insert(this->columns, at, (Object*) ListItem_new(text, 0));
         ColumnsListBox_update(this->columns);
         result = HANDLED;
         break;
      }
   }
   return result;
}
