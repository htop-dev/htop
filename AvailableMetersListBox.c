
#include "AvailableMetersListBox.h"
#include "Settings.h"
#include "Header.h"
#include "ScreenManager.h"

#include "ListBox.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct AvailableMetersListBox_ {
   ListBox super;

   Settings* settings;
   ListBox* leftBox;
   ListBox* rightBox;
   ScreenManager* scr;
} AvailableMetersListBox;

}*/

AvailableMetersListBox* AvailableMetersListBox_new(Settings* settings, ListBox* leftMeters, ListBox* rightMeters, ScreenManager* scr) {
   AvailableMetersListBox* this = (AvailableMetersListBox*) malloc(sizeof(AvailableMetersListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = AvailableMetersListBox_delete;
   
   this->settings = settings;
   this->leftBox = leftMeters;
   this->rightBox = rightMeters;
   this->scr = scr;
   super->eventHandler = AvailableMetersListBox_EventHandler;

   ListBox_setHeader(super, "Available meters");
   for (int i = 1; Meter_types[i]; i++) {
      MeterType* type = Meter_types[i];
      if (type != &CPUMeter) {
         ListBox_add(super, (Object*) ListItem_new(type->uiName, i << 16));
      }
   }
   MeterType* type = &CPUMeter;
   int processors = settings->pl->processorCount;
   if (processors > 1) {
      ListBox_add(super, (Object*) ListItem_new("CPU average", 0));
      for (int i = 1; i <= processors; i++) {
         char buffer[50];
         sprintf(buffer, "%s %d", type->uiName, i);
         ListBox_add(super, (Object*) ListItem_new(buffer, i));
      }
   } else {
      ListBox_add(super, (Object*) ListItem_new("CPU", 1));
   }
   return this;
}

void AvailableMetersListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   AvailableMetersListBox* this = (AvailableMetersListBox*) object;
   ListBox_done(super);
   free(this);
}

/* private */
inline void AvailableMetersListBox_addHeader(Header* header, ListBox* lb, MeterType* type, int param, HeaderSide side) {
   Meter* meter = (Meter*) Header_addMeter(header, type, param, side);
   ListBox_add(lb, (Object*) Meter_toListItem(meter));
}

HandlerResult AvailableMetersListBox_EventHandler(ListBox* super, int ch) {
   AvailableMetersListBox* this = (AvailableMetersListBox*) super;
   Header* header = this->settings->header;
   
   ListItem* selected = (ListItem*) ListBox_getSelected(super);
   int param = selected->key & 0xff;
   int type = selected->key >> 16;
   HandlerResult result = IGNORED;

   switch(ch) {
      case KEY_F(5):
      case 'l':
      case 'L':
      {
         AvailableMetersListBox_addHeader(header, this->leftBox, Meter_types[type], param, LEFT_HEADER);
         result = HANDLED;
         break;
      }
      case KEY_F(6):
      case 'r':
      case 'R':
      {
         AvailableMetersListBox_addHeader(header, this->rightBox, Meter_types[type], param, RIGHT_HEADER);
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED) {
      this->settings->changed = true;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}
