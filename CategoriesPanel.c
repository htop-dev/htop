
#include "CategoriesPanel.h"
#include "AvailableMetersPanel.h"
#include "MetersPanel.h"
#include "DisplayOptionsPanel.h"
#include "ColumnsPanel.h"
#include "ColorsPanel.h"
#include "AvailableColumnsPanel.h"

#include "Panel.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct CategoriesPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} CategoriesPanel;

}*/

static char* MetersFunctions[10] = {"      ", "      ", "      ", "Type  ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  "};

static char* AvailableMetersFunctions[10] = {"      ", "      ", "      ", "      ", "Add L ", "Add R ", "      ", "      ", "      ", "Done  "};

static char* DisplayOptionsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  "};

static char* ColumnsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  "};

static char* ColorsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  "};

static char* AvailableColumnsFunctions[10] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  "};

static void CategoriesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   CategoriesPanel* this = (CategoriesPanel*) object;
   Panel_done(super);
   free(this);
}

void CategoriesPanel_makeMetersPage(CategoriesPanel* this) {
   Panel* leftMeters = (Panel*) MetersPanel_new(this->settings, "Left column", this->settings->header->leftMeters, this->scr);
   Panel* rightMeters = (Panel*) MetersPanel_new(this->settings, "Right column", this->settings->header->rightMeters, this->scr);
   Panel* availableMeters = (Panel*) AvailableMetersPanel_new(this->settings, leftMeters, rightMeters, this->scr);
   ScreenManager_add(this->scr, leftMeters, FunctionBar_new(10, MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, rightMeters, FunctionBar_new(10, MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, availableMeters, FunctionBar_new(10, AvailableMetersFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeDisplayOptionsPage(CategoriesPanel* this) {
   Panel* displayOptions = (Panel*) DisplayOptionsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, displayOptions, FunctionBar_new(10, DisplayOptionsFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeColorsPage(CategoriesPanel* this) {
   Panel* colors = (Panel*) ColorsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, colors, FunctionBar_new(10, ColorsFunctions, NULL, NULL), -1);
}

static void CategoriesPanel_makeColumnsPage(CategoriesPanel* this) {
   Panel* columns = (Panel*) ColumnsPanel_new(this->settings, this->scr);
   Panel* availableColumns = (Panel*) AvailableColumnsPanel_new(this->settings, columns, this->scr);
   ScreenManager_add(this->scr, columns, FunctionBar_new(10, ColumnsFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, availableColumns, FunctionBar_new(10, AvailableColumnsFunctions, NULL, NULL), -1);
}

static HandlerResult CategoriesPanel_eventHandler(Panel* super, int ch) {
   CategoriesPanel* this = (CategoriesPanel*) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
      case EVENT_SETSELECTED:
         result = HANDLED;
         break;
      case KEY_UP:
      case KEY_DOWN:
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END: {
         int previous = selected;
         Panel_onKey(super, ch);
         selected = Panel_getSelectedIndex(super);
         if (previous != selected)
            result = HANDLED;
         break;
      }
   }

   if (result == HANDLED) {
      int size = ScreenManager_size(this->scr);
      for (int i = 1; i < size; i++)
         ScreenManager_remove(this->scr, 1);
      switch (selected) {
         case 0:
            CategoriesPanel_makeMetersPage(this);
            break;
         case 1:
            CategoriesPanel_makeDisplayOptionsPage(this);
            break;
         case 2:
            CategoriesPanel_makeColorsPage(this);
            break;
         case 3:
            CategoriesPanel_makeColumnsPage(this);
            break;
      }
   }

   return result;
}

CategoriesPanel* CategoriesPanel_new(Settings* settings, ScreenManager* scr) {
   CategoriesPanel* this = (CategoriesPanel*) malloc(sizeof(CategoriesPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = CategoriesPanel_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = CategoriesPanel_eventHandler;
   Panel_setHeader(super, "Setup");
   Panel_add(super, (Object*) ListItem_new("Meters", 0));
   Panel_add(super, (Object*) ListItem_new("Display options", 0));
   Panel_add(super, (Object*) ListItem_new("Colors", 0));
   Panel_add(super, (Object*) ListItem_new("Columns", 0));
   return this;
}
