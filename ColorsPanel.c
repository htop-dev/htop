/*
htop - ColorsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ColorsPanel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Object.h"
#include "OptionItem.h"
#include "ProvideCurses.h"


// TO ADD A NEW SCHEME:
// * Increment the size of bool check in ColorsPanel.h
// * Add the entry in the ColorSchemeNames array below in the file
// * Add a define in CRT.h that matches the order of the array
// * Add the colors in CRT_setColors


static const char* const ColorsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static const char* const ColorSchemeNames[] = {
   "Default",
   "Monochromatic",
   "Black on White",
   "Light Terminal",
   "MC",
   "Black Night",
   "Broken Gray",
   NULL
};

static void ColorsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ColorsPanel* this = (ColorsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ColorsPanel_eventHandler(Panel* super, int ch) {
   ColorsPanel* this = (ColorsPanel*) super;

   HandlerResult result = IGNORED;

   switch (ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
      case ' ': {
         int mark = Panel_getSelectedIndex(super);
         assert(mark >= 0);
         assert(mark < LAST_COLORSCHEME);

         for (int i = 0; ColorSchemeNames[i] != NULL; i++)
            CheckItem_set((CheckItem*)Panel_get(super, i), false);
         CheckItem_set((CheckItem*)Panel_get(super, mark), true);

         this->settings->colorScheme = mark;
         this->settings->changed = true;
         this->settings->lastUpdate++;

         CRT_setColors(mark);
         clear();

         result = HANDLED | REDRAW;
      }
   }

   return result;
}

const PanelClass ColorsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ColorsPanel_delete
   },
   .eventHandler = ColorsPanel_eventHandler
};

ColorsPanel* ColorsPanel_new(Settings* settings) {
   ColorsPanel* this = AllocThis(ColorsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ColorsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;

   assert(ARRAYSIZE(ColorSchemeNames) == LAST_COLORSCHEME + 1);

   Panel_setHeader(super, "Colors");
   for (int i = 0; ColorSchemeNames[i] != NULL; i++) {
      Panel_add(super, (Object*) CheckItem_newByVal(ColorSchemeNames[i], false));
   }
   CheckItem_set((CheckItem*)Panel_get(super, settings->colorScheme), true);
   return this;
}
