
#include "config.h"
#include "CRT.h"
#include "ColorsPanel.h"

#include "Panel.h"
#include "CheckItem.h"
#include "Settings.h"
#include "ScreenManager.h"

#include "debug.h"
#include <assert.h>

// TO ADD A NEW SCHEME:
// * Increment the size of bool check in ColorsPanel.h
// * Add the entry in the ColorSchemes array below in the file
// * Add a define in CRT.h that matches the order of the array
// * Add the colors in CRT_setColors

/*{

typedef struct ColorsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} ColorsPanel;

}*/

static const char* ColorSchemes[] = {
   "Default",
   "Monochromatic",
   "Black on White",
   "Light Terminal",
   "MC",
   "Black Night",
   NULL
};

static void ColorsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ColorsPanel* this = (ColorsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ColorsPanel_EventHandler(Panel* super, int ch) {
   ColorsPanel* this = (ColorsPanel*) super;
   
   HandlerResult result = IGNORED;
   int mark = Panel_getSelectedIndex(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case ' ':
      for (int i = 0; ColorSchemes[i] != NULL; i++)
         CheckItem_set((CheckItem*)Panel_get(super, i), false);
      CheckItem_set((CheckItem*)Panel_get(super, mark), true);
      this->settings->colorScheme = mark;
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header* header = this->settings->header;
      CRT_setColors(mark);
      Panel* menu = (Panel*) Vector_get(this->scr->panels, 0);
      Header_draw(header);
      RichString_setAttr(&(super->header), CRT_colors[PANEL_HEADER_FOCUS]);
      RichString_setAttr(&(menu->header), CRT_colors[PANEL_HEADER_UNFOCUS]);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

ColorsPanel* ColorsPanel_new(Settings* settings, ScreenManager* scr) {
   ColorsPanel* this = (ColorsPanel*) malloc(sizeof(ColorsPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, CHECKITEM_CLASS, true);
   ((Object*)this)->delete = ColorsPanel_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = ColorsPanel_EventHandler;

   Panel_setHeader(super, "Colors");
   for (int i = 0; ColorSchemes[i] != NULL; i++) {
      Panel_add(super, (Object*) CheckItem_new(strdup(ColorSchemes[i]), NULL, false));
   }
   CheckItem_set((CheckItem*)Panel_get(super, settings->colorScheme), true);
   return this;
}
