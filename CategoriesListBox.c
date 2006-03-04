
#include "CategoriesListBox.h"
#include "AvailableMetersListBox.h"
#include "MetersListBox.h"
#include "DisplayOptionsListBox.h"
#include "ColumnsListBox.h"
#include "ColorsListBox.h"
#include "AvailableColumnsListBox.h"

#include "ListBox.h"

#include "debug.h"
#include <assert.h>

/*{

typedef struct CategoriesListBox_ {
   ListBox super;

   Settings* settings;
   ScreenManager* scr;
} CategoriesListBox;

}*/

/* private property */
char* MetersFunctions[10] = {"      ", "      ", "      ", "Type  ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  "};

/* private property */
char* AvailableMetersFunctions[10] = {"      ", "      ", "      ", "      ", "Add L ", "Add R ", "      ", "      ", "      ", "Done  "};

/* private property */
char* DisplayOptionsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  "};

/* private property */
char* ColumnsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  "};

/* private property */
char* ColorsFunctions[10] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  "};

/* private property */
char* AvailableColumnsFunctions[10] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  "};

CategoriesListBox* CategoriesListBox_new(Settings* settings, ScreenManager* scr) {
   CategoriesListBox* this = (CategoriesListBox*) malloc(sizeof(CategoriesListBox));
   ListBox* super = (ListBox*) this;
   ListBox_init(super, 1, 1, 1, 1, LISTITEM_CLASS, true);
   ((Object*)this)->delete = CategoriesListBox_delete;

   this->settings = settings;
   this->scr = scr;
   super->eventHandler = CategoriesListBox_eventHandler;
   ListBox_setHeader(super, "Setup");
   ListBox_add(super, (Object*) ListItem_new("Meters", 0));
   ListBox_add(super, (Object*) ListItem_new("Display options", 0));
   ListBox_add(super, (Object*) ListItem_new("Colors", 0));
   ListBox_add(super, (Object*) ListItem_new("Columns", 0));
   return this;
}

void CategoriesListBox_delete(Object* object) {
   ListBox* super = (ListBox*) object;
   CategoriesListBox* this = (CategoriesListBox*) object;
   ListBox_done(super);
   free(this);
}

HandlerResult CategoriesListBox_eventHandler(ListBox* super, int ch) {
   CategoriesListBox* this = (CategoriesListBox*) super;

   HandlerResult result = IGNORED;

   int previous = ListBox_getSelectedIndex(super);

   switch (ch) {
      case KEY_UP:
      case KEY_DOWN:
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END: {
         ListBox_onKey(super, ch);
         int selected = ListBox_getSelectedIndex(super);
         if (previous != selected) {
            int size = ScreenManager_size(this->scr);
            for (int i = 1; i < size; i++)
               ScreenManager_remove(this->scr, 1);
            switch (selected) {
               case 0:
                  CategoriesListBox_makeMetersPage(this);
                  break;
               case 1:
                  CategoriesListBox_makeDisplayOptionsPage(this);
                  break;
               case 2:
                  CategoriesListBox_makeColorsPage(this);
                  break;
               case 3:
                  CategoriesListBox_makeColumnsPage(this);
                  break;
            }
         }
         result = HANDLED;
      }
   }

   return result;
}

void CategoriesListBox_makeMetersPage(CategoriesListBox* this) {
   ListBox* lbLeftMeters = (ListBox*) MetersListBox_new(this->settings, "Left column", this->settings->header->leftMeters, this->scr);
   ListBox* lbRightMeters = (ListBox*) MetersListBox_new(this->settings, "Right column", this->settings->header->rightMeters, this->scr);
   ListBox* lbAvailableMeters = (ListBox*) AvailableMetersListBox_new(this->settings, lbLeftMeters, lbRightMeters, this->scr);
   ScreenManager_add(this->scr, lbLeftMeters, FunctionBar_new(10, MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, lbRightMeters, FunctionBar_new(10, MetersFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, lbAvailableMeters, FunctionBar_new(10, AvailableMetersFunctions, NULL, NULL), -1);
}

void CategoriesListBox_makeDisplayOptionsPage(CategoriesListBox* this) {
   ListBox* lbDisplayOptions = (ListBox*) DisplayOptionsListBox_new(this->settings, this->scr);
   ScreenManager_add(this->scr, lbDisplayOptions, FunctionBar_new(10, DisplayOptionsFunctions, NULL, NULL), -1);
}

void CategoriesListBox_makeColorsPage(CategoriesListBox* this) {
   ListBox* lbColors = (ListBox*) ColorsListBox_new(this->settings, this->scr);
   ScreenManager_add(this->scr, lbColors, FunctionBar_new(10, ColorsFunctions, NULL, NULL), -1);
}

void CategoriesListBox_makeColumnsPage(CategoriesListBox* this) {
   ListBox* lbColumns = (ListBox*) ColumnsListBox_new(this->settings, this->scr);
   ListBox* lbAvailableColumns = (ListBox*) AvailableColumnsListBox_new(this->settings, lbColumns, this->scr);
   ScreenManager_add(this->scr, lbColumns, FunctionBar_new(10, ColumnsFunctions, NULL, NULL), 20);
   ScreenManager_add(this->scr, lbAvailableColumns, FunctionBar_new(10, AvailableColumnsFunctions, NULL, NULL), -1);
}
