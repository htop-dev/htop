/*
htop - MetersPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MetersPanel.h"

#include <stdlib.h>
#include <assert.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct MetersPanel_ {
   Panel super;

   Settings* settings;
   Vector* meters;
   ScreenManager* scr;
} MetersPanel;

}*/

static void MetersPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   MetersPanel* this = (MetersPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult MetersPanel_eventHandler(Panel* super, int ch) {
   MetersPanel* this = (MetersPanel*) super;
   
   int selected = Panel_getSelectedIndex(super);
   HandlerResult result = IGNORED;

   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_F(4):
      case 't':
      {
         Meter* meter = (Meter*) Vector_get(this->meters, selected);
         int mode = meter->mode + 1;
         if (mode == LAST_METERMODE) mode = 1;
         Meter_setMode(meter, mode);
         Panel_set(super, selected, (Object*) Meter_toListItem(meter));
         result = HANDLED;
         break;
      }
      case KEY_F(7):
      case '[':
      case '-':
      {
         Vector_moveUp(this->meters, selected);
         Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
         Vector_moveDown(this->meters, selected);
         Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      case KEY_DC:
      {
         if (selected < Vector_size(this->meters)) {
            Vector_remove(this->meters, selected);
            Panel_remove(super, selected);
         }
         result = HANDLED;
         break;
      }
   }
   if (result == HANDLED) {
      Header* header = this->settings->header;
      this->settings->changed = true;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

MetersPanel* MetersPanel_new(Settings* settings, const char* header, Vector* meters, ScreenManager* scr) {
   MetersPanel* this = (MetersPanel*) malloc(sizeof(MetersPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = MetersPanel_delete;

   this->settings = settings;
   this->meters = meters;
   this->scr = scr;
   super->eventHandler = MetersPanel_eventHandler;
   Panel_setHeader(super, header);
   for (int i = 0; i < Vector_size(meters); i++) {
      Meter* meter = (Meter*) Vector_get(meters, i);
      Panel_add(super, (Object*) Meter_toListItem(meter));
   }
   return this;
}
