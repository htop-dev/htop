
#include "AvailableMetersPanel.h"
#include "Settings.h"
#include "Header.h"
#include "ScreenManager.h"
#include "CPUMeter.h"

#include "Panel.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct AvailableMetersPanel_ {
   Panel super;

   Settings* settings;
   Panel* leftPanel;
   Panel* rightPanel;
   ScreenManager* scr;
} AvailableMetersPanel;

}*/

static void AvailableMetersPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableMetersPanel* this = (AvailableMetersPanel*) object;
   Panel_done(super);
   free(this);
}

static inline void AvailableMetersPanel_addHeader(Header* header, Panel* panel, MeterType* type, int param, HeaderSide side) {
   Meter* meter = (Meter*) Header_addMeter(header, type, param, side);
   Panel_add(panel, (Object*) Meter_toListItem(meter));
}

static HandlerResult AvailableMetersPanel_eventHandler(Panel* super, int ch) {
   AvailableMetersPanel* this = (AvailableMetersPanel*) super;
   Header* header = this->settings->header;
   
   ListItem* selected = (ListItem*) Panel_getSelected(super);
   int param = selected->key & 0xff;
   int type = selected->key >> 16;
   HandlerResult result = IGNORED;

   switch(ch) {
      case KEY_F(5):
      case 'l':
      case 'L':
      {
         AvailableMetersPanel_addHeader(header, this->leftPanel, Meter_types[type], param, LEFT_HEADER);
         result = HANDLED;
         break;
      }
      case KEY_F(6):
      case 'r':
      case 'R':
      {
         AvailableMetersPanel_addHeader(header, this->rightPanel, Meter_types[type], param, RIGHT_HEADER);
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

AvailableMetersPanel* AvailableMetersPanel_new(Settings* settings, Panel* leftMeters, Panel* rightMeters, ScreenManager* scr) {
   AvailableMetersPanel* this = (AvailableMetersPanel*) malloc(sizeof(AvailableMetersPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = AvailableMetersPanel_delete;
   
   this->settings = settings;
   this->leftPanel = leftMeters;
   this->rightPanel = rightMeters;
   this->scr = scr;
   super->eventHandler = AvailableMetersPanel_eventHandler;

   Panel_setHeader(super, "Available meters");
   for (int i = 1; Meter_types[i]; i++) {
      MeterType* type = Meter_types[i];
      if (type != &CPUMeter) {
         Panel_add(super, (Object*) ListItem_new(type->uiName, i << 16));
      }
   }
   MeterType* type = &CPUMeter;
   int processors = settings->pl->processorCount;
   if (processors > 1) {
      Panel_add(super, (Object*) ListItem_new("CPU average", 0));
      for (int i = 1; i <= processors; i++) {
         char buffer[50];
         sprintf(buffer, "%s %d", type->uiName, i);
         Panel_add(super, (Object*) ListItem_new(buffer, i));
      }
   } else {
      Panel_add(super, (Object*) ListItem_new("CPU", 1));
   }
   return this;
}
