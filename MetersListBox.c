
#include "MetersListBox.h"

#include "ListBox.h"
#include "Settings.h"
#include "ScreenManager.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct MetersListBox_ {
   ListBox super;

   Settings* settings;
   TypedVector* meters;
   ScreenManager* scr;
} MetersListBox;

}*/

MetersListBox* MetersListBox_new(Settings* settings, char* header, TypedVector* meters, ScreenManager* scr) {
   MetersListBox* this = (MetersListBox*) malloc(sizeof(MetersListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = MetersListBox_delete;

   this->settings = settings;
   this->meters = meters;
   this->scr = scr;
   super->eventHandler = MetersListBox_EventHandler;
   ListBox_setHeader(super, header);
   for (int i = 0; i < TypedVector_size(meters); i++) {
      Meter* meter = (Meter*) TypedVector_get(meters, i);
      ListBox_add(super, (Object*) Meter_toListItem(meter));
   }
   return this;
}

void MetersListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   MetersListBox* this = (MetersListBox*) object;
   ListBox_done(super);
   free(this);
}

HandlerResult MetersListBox_EventHandler(ListBox* super, int ch) {
   MetersListBox* this = (MetersListBox*) super;
   
   int selected = ListBox_getSelectedIndex(super);
   HandlerResult result = IGNORED;

   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_F(4):
      case 't':
      {
         Meter* meter = (Meter*) TypedVector_get(this->meters, selected);
         MeterMode mode = meter->mode + 1;
         if (mode == LAST_METERMODE)
            mode = 1; // skip mode 0, "unset"
         Meter_setMode(meter, mode);
         ListBox_set(super, selected, (Object*) Meter_toListItem(meter));
         result = HANDLED;
         break;
      }
      case KEY_F(7):
      case '[':
      case '-':
      {
         TypedVector_moveUp(this->meters, selected);
         ListBox_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
         TypedVector_moveDown(this->meters, selected);
         ListBox_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      case KEY_DC:
      {
         if (selected < TypedVector_size(this->meters)) {
            TypedVector_remove(this->meters, selected);
            ListBox_remove(super, selected);
         }
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED) {
      Header* header = this->settings->header;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}
