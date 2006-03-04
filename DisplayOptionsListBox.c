
#include "DisplayOptionsListBox.h"

#include "ListBox.h"
#include "CheckItem.h"
#include "Settings.h"
#include "ScreenManager.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct DisplayOptionsListBox_ {
   ListBox super;

   Settings* settings;
   ScreenManager* scr;
} DisplayOptionsListBox;

}*/

DisplayOptionsListBox* DisplayOptionsListBox_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsListBox* this = (DisplayOptionsListBox*) malloc(sizeof(DisplayOptionsListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, CHECKITEM_CLASS, true);
   ((Object*)this)->delete = DisplayOptionsListBox_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = DisplayOptionsListBox_EventHandler;

   ListBox_setHeader(super, "Display options");
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Tree view"), &(settings->pl->treeView)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Shadow other users' processes"), &(settings->pl->shadowOtherUsers)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Hide kernel threads"), &(settings->pl->hideKernelThreads)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Hide userland threads"), &(settings->pl->hideUserlandThreads)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Highlight program \"basename\""), &(settings->pl->highlightBaseName)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Highlight megabytes in memory counters"), &(settings->pl->highlightMegabytes)));
   ListBox_add(super, (Object*) CheckItem_new(String_copy("Leave a margin around header"), &(settings->header->margin)));
   return this;
}

void DisplayOptionsListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   DisplayOptionsListBox* this = (DisplayOptionsListBox*) object;
   ListBox_done(super);
   free(this);
}

HandlerResult DisplayOptionsListBox_EventHandler(ListBox* super, int ch) {
   DisplayOptionsListBox* this = (DisplayOptionsListBox*) super;
   
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) ListBox_getSelected(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case ' ':
      *(selected->value) = ! *(selected->value);
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header* header = this->settings->header;
      Header_calculateHeight(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

