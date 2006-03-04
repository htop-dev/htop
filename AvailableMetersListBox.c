
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
   ListBox_add(super, (Object*) ListItem_new("Swap", 0));
   ListBox_add(super, (Object*) ListItem_new("Memory", 0));
   ListBox_add(super, (Object*) ListItem_new("Clock", 0));
   ListBox_add(super, (Object*) ListItem_new("Load", 0));
   ListBox_add(super, (Object*) ListItem_new("LoadAverage", 0));
   ListBox_add(super, (Object*) ListItem_new("Uptime", 0));
   ListBox_add(super, (Object*) ListItem_new("Tasks", 0));
   if (settings->pl->processorCount > 1)
      ListBox_add(super, (Object*) ListItem_new("CPUAverage", 0));
   for (int i = 1; i <= settings->pl->processorCount; i++) {
      char buffer[50];
      sprintf(buffer, "CPU(%d)", i);
      ListBox_add(super, (Object*) ListItem_new(buffer, 0));
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
inline void AvailableMetersListBox_addHeader(Header* header, ListBox* lb, char* name, HeaderSide side) {
   Header_createMeter(header, name, side);
   int i = Header_size(header, side) - 1;
   Meter* meter = (Meter*) Header_getMeter(header, i, side);
   ListBox_add(lb, (Object*) Meter_toListItem(meter));
}

HandlerResult AvailableMetersListBox_EventHandler(ListBox* super, int ch) {
   AvailableMetersListBox* this = (AvailableMetersListBox*) super;
   Header* header = this->settings->header;
   
   ListItem* selected = (ListItem*) ListBox_getSelected(super);
   char* name = selected->value;
   HandlerResult result = IGNORED;

   switch(ch) {
      case KEY_F(5):
      case 'l':
      case 'L':
      {
         AvailableMetersListBox_addHeader(header, this->leftBox, name, LEFT_HEADER);
         result = HANDLED;
         break;
      }
      case KEY_F(6):
      case 'r':
      case 'R':
      {
         AvailableMetersListBox_addHeader(header, this->rightBox, name, RIGHT_HEADER);
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
