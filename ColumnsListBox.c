
#include "ColumnsListBox.h"

#include "ListBox.h"
#include "Settings.h"
#include "ScreenManager.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct ColumnsListBox_ {
   ListBox super;

   Settings* settings;
   ScreenManager* scr;
} ColumnsListBox;

}*/

ColumnsListBox* ColumnsListBox_new(Settings* settings, ScreenManager* scr) {
   ColumnsListBox* this = (ColumnsListBox*) malloc(sizeof(ColumnsListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = ColumnsListBox_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = ColumnsListBox_eventHandler;
   ListBox_setHeader(super, "Active Columns");

   ProcessField* fields = this->settings->pl->fields;
   for (; *fields; fields++) {
      ListBox_add(super, (Object*) ListItem_new(Process_fieldNames[*fields], 0));
   }
   return this;
}

void ColumnsListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   ColumnsListBox* this = (ColumnsListBox*) object;
   ListBox_done(super);
   free(this);
}

void ColumnsListBox_update(ListBox* super) {
   ColumnsListBox* this = (ColumnsListBox*) super;
   int size = ListBox_getSize(super);
   this->settings->changed = true;
   // FIXME: this is crappily inefficient
   free(this->settings->pl->fields);
   this->settings->pl->fields = (ProcessField*) malloc(sizeof(ProcessField) * (size+1));
   for (int i = 0; i < size; i++) {
      char* text = ((ListItem*) ListBox_get(super, i))->value;
      for (int j = 1; j <= LAST_PROCESSFIELD; j++) {
         if (String_eq(text, Process_fieldNames[j])) {
            this->settings->pl->fields[i] = j;
            break;
         }
      }
   }
   this->settings->pl->fields[size] = 0;
}

HandlerResult ColumnsListBox_eventHandler(ListBox* super, int ch) {
   
   int selected = ListBox_getSelectedIndex(super);
   HandlerResult result = IGNORED;
   int size = ListBox_getSize(super);

   switch(ch) {
      case KEY_F(7):
      case '[':
      case '-':
      {
         if (selected < size - 1)
            ListBox_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
	 if (selected < size - 2) 
            ListBox_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      case KEY_DC:
      {
         if (selected < size - 1) {
            ListBox_remove(super, selected);
         }
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED)
      ColumnsListBox_update(super);
   return result;
}
