
#include "CRT.h"
#include "ColorsListBox.h"

#include "ListBox.h"
#include "CheckItem.h"
#include "Settings.h"
#include "ScreenManager.h"

#include "debug.h"
#include <assert.h>

// TO ADD A NEW SCHEME:
// * Increment the size of bool check in ColorsListBox.h
// * Add the entry in the ColorSchemes array below in the file
// * Add a define in CRT.h that matches the order of the array
// * Add the colors in CRT_setColors

/*{

typedef struct ColorsListBox_ {
   ListBox super;

   Settings* settings;
   ScreenManager* scr;
   bool check[5];
} ColorsListBox;

}*/

/* private */
static char* ColorSchemes[] = {
   "Default",
   "Monochromatic",
   "Black on White",
   "Light Terminal",
   "MC",
   "Black Night",
   NULL
};

ColorsListBox* ColorsListBox_new(Settings* settings, ScreenManager* scr) {
   ColorsListBox* this = (ColorsListBox*) malloc(sizeof(ColorsListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, CHECKITEM_CLASS, true);
   ((Object*)this)->delete = ColorsListBox_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = ColorsListBox_EventHandler;

   ListBox_setHeader(super, "Colors");
   for (int i = 0; ColorSchemes[i] != NULL; i++) {
      ListBox_add(super, (Object*) CheckItem_new(String_copy(ColorSchemes[i]), &(this->check[i])));
      this->check[i] = false;
   }
   this->check[settings->colorScheme] = true;
   return this;
}

void ColorsListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   ColorsListBox* this = (ColorsListBox*) object;
   ListBox_done(super);
   free(this);
}

HandlerResult ColorsListBox_EventHandler(ListBox* super, int ch) {
   ColorsListBox* this = (ColorsListBox*) super;
   
   HandlerResult result = IGNORED;
   int mark = ListBox_getSelectedIndex(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case ' ':
      for (int i = 0; ColorSchemes[i] != NULL; i++) {
         this->check[i] = false;
      }
      this->check[mark] = true;
      this->settings->colorScheme = mark;
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header* header = this->settings->header;
      CRT_setColors(mark);
      ListBox* lbMenu = (ListBox*) TypedVector_get(this->scr->items, 0);
      Header_draw(header);
      RichString_setAttr(&(super->header), CRT_colors[PANEL_HEADER_FOCUS]);
      RichString_setAttr(&(lbMenu->header), CRT_colors[PANEL_HEADER_UNFOCUS]);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

