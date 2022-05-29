/*
htop - HeaderOptionsPanel.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "HeaderOptionsPanel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Header.h"
#include "HeaderLayout.h"
#include "Object.h"
#include "OptionItem.h"
#include "ProvideCurses.h"


static const char* const HeaderOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void HeaderOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   HeaderOptionsPanel* this = (HeaderOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult HeaderOptionsPanel_eventHandler(Panel* super, int ch) {
   HeaderOptionsPanel* this = (HeaderOptionsPanel*) super;

   HandlerResult result = IGNORED;
   int mark;

   switch (ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      mark = Panel_getSelectedIndex(super);
      assert(mark >= 0);
      assert(mark < LAST_HEADER_LAYOUT);

      for (int i = 0; i < LAST_HEADER_LAYOUT; i++)
         CheckItem_set((CheckItem*)Panel_get(super, i), false);
      CheckItem_set((CheckItem*)Panel_get(super, mark), true);

      Header_setLayout(this->scr->header, mark);
      this->settings->changed = true;
      this->settings->lastUpdate++;

      ScreenManager_resize(this->scr);

      result = HANDLED;
   }

   return result;
}

const PanelClass HeaderOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = HeaderOptionsPanel_delete
   },
   .eventHandler = HeaderOptionsPanel_eventHandler
};

HeaderOptionsPanel* HeaderOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   HeaderOptionsPanel* this = AllocThis(HeaderOptionsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(HeaderOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->scr = scr;
   this->settings = settings;

   Panel_setHeader(super, "Header Layout");
   for (int i = 0; i < LAST_HEADER_LAYOUT; i++) {
      Panel_add(super, (Object*) CheckItem_newByVal(HeaderLayout_layouts[i].description, false));
   }
   CheckItem_set((CheckItem*)Panel_get(super, scr->header->headerLayout), true);
   return this;
}
