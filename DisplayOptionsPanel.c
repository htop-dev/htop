/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DisplayOptionsPanel.h"

#include "CheckItem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct DisplayOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} DisplayOptionsPanel;

}*/

static const char* DisplayOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void DisplayOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) super;
   
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case ' ':
      CheckItem_set(selected, ! (CheckItem_get(selected)) );
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      const Header* header = this->scr->header;
      Header_calculateHeight((Header*) header);
      Header_reinit((Header*) header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

PanelClass DisplayOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DisplayOptionsPanel_delete
   },
   .eventHandler = DisplayOptionsPanel_eventHandler
};

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsPanel* this = AllocThis(DisplayOptionsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(DisplayOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Display options");
   Panel_add(super, (Object*) CheckItem_new(strdup("Tree view"), &(settings->treeView), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Shadow other users' processes"), &(settings->shadowOtherUsers), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Hide kernel threads"), &(settings->hideKernelThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Hide userland threads"), &(settings->hideUserlandThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Display threads in a different color"), &(settings->highlightThreads), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Show custom thread names"), &(settings->showThreadNames), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Highlight program \"basename\""), &(settings->highlightBaseName), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Highlight large numbers in memory counters"), &(settings->highlightMegabytes), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Leave a margin around header"), &(settings->headerMargin), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)"), &(settings->detailedCPUTime), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Count CPUs from 0 instead of 1"), &(settings->countCPUsFromZero), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Update process names on every refresh"), &(settings->updateProcessNames), false));
   Panel_add(super, (Object*) CheckItem_new(strdup("Add guest time in CPU meter percentage"), &(settings->accountGuestInCPUMeter), false));
   return this;
}
