/*
htop - CategoriesPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CategoriesPanel.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "AvailableMetersPanel.h"
#include "ColorsPanel.h"
#include "DisplayOptionsPanel.h"
#include "FunctionBar.h"
#include "Header.h"
#include "HeaderLayout.h"
#include "HeaderOptionsPanel.h"
#include "ListItem.h"
#include "Macros.h"
#include "MetersPanel.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "ScreensPanel.h"
#include "ScreenTabsPanel.h"
#include "Settings.h"
#include "Vector.h"
#include "XUtils.h"


static const char* const CategoriesFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void CategoriesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   CategoriesPanel* this = (CategoriesPanel*) object;
   Panel_done(super);
   free(this);
}

static void CategoriesPanel_makeMetersPage(CategoriesPanel* this) {
   size_t columns = HeaderLayout_getColumns(this->scr->header->headerLayout);
   MetersPanel** meterPanels = xMallocArray(columns, sizeof(MetersPanel*));
   Settings* settings = this->host->settings;

   for (size_t i = 0; i < columns; i++) {
      char titleBuffer[32];
      xSnprintf(titleBuffer, sizeof(titleBuffer), "Column %zu", i + 1);
      meterPanels[i] = MetersPanel_new(settings, titleBuffer, this->header->columns[i], this->scr);

      if (i != 0) {
         meterPanels[i]->leftNeighbor = meterPanels[i - 1];
         meterPanels[i - 1]->rightNeighbor = meterPanels[i];
      }

      ScreenManager_add(this->scr, (Panel*) meterPanels[i], 20);
   }

   Panel* availableMeters = (Panel*) AvailableMetersPanel_new(this->host, this->header, columns, meterPanels, this->scr);
   ScreenManager_add(this->scr, availableMeters, -1);
}

static void CategoriesPanel_makeDisplayOptionsPage(CategoriesPanel* this) {
   Settings* settings = this->host->settings;
   Panel* displayOptions = (Panel*) DisplayOptionsPanel_new(settings, this->scr);
   ScreenManager_add(this->scr, displayOptions, -1);
}

static void CategoriesPanel_makeColorsPage(CategoriesPanel* this) {
   Settings* settings = this->host->settings;
   Panel* colors = (Panel*) ColorsPanel_new(settings);
   ScreenManager_add(this->scr, colors, -1);
}

#if defined(HTOP_PCP)   /* all platforms supporting dynamic screens */
static void CategoriesPanel_makeScreenTabsPage(CategoriesPanel* this) {
   Settings* settings = this->host->settings;
   Panel* screenTabs = (Panel*) ScreenTabsPanel_new(settings);
   Panel* screenNames = (Panel*) ((ScreenTabsPanel*)screenTabs)->names;
   ScreenManager_add(this->scr, screenTabs, 20);
   ScreenManager_add(this->scr, screenNames, -1);
}
#endif

static void CategoriesPanel_makeScreensPage(CategoriesPanel* this) {
   Settings* settings = this->host->settings;
   Panel* screens = (Panel*) ScreensPanel_new(settings);
   Panel* columns = (Panel*) ((ScreensPanel*)screens)->columns;
   Panel* availableColumns = (Panel*) ((ScreensPanel*)screens)->availableColumns;
   ScreenManager_add(this->scr, screens, 20);
   ScreenManager_add(this->scr, columns, 20);
   ScreenManager_add(this->scr, availableColumns, -1);
}

static void CategoriesPanel_makeHeaderOptionsPage(CategoriesPanel* this) {
   Settings* settings = this->host->settings;
   Panel* colors = (Panel*) HeaderOptionsPanel_new(settings, this->scr);
   ScreenManager_add(this->scr, colors, -1);
}

typedef void (* CategoriesPanel_makePageFunc)(CategoriesPanel* ref);
typedef struct CategoriesPanelPage_ {
   const char* name;
   CategoriesPanel_makePageFunc ctor;
} CategoriesPanelPage;

static CategoriesPanelPage categoriesPanelPages[] = {
   { .name = "Display options", .ctor = CategoriesPanel_makeDisplayOptionsPage },
   { .name = "Header layout", .ctor = CategoriesPanel_makeHeaderOptionsPage },
   { .name = "Meters", .ctor = CategoriesPanel_makeMetersPage },
#if defined(HTOP_PCP)   /* all platforms supporting dynamic screens */
   { .name = "Screen tabs", .ctor = CategoriesPanel_makeScreenTabsPage },
#endif
   { .name = "Screens", .ctor = CategoriesPanel_makeScreensPage },
   { .name = "Colors", .ctor = CategoriesPanel_makeColorsPage },
};

static HandlerResult CategoriesPanel_eventHandler(Panel* super, int ch) {
   CategoriesPanel* this = (CategoriesPanel*) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_UP:
      case KEY_CTRL('P'):
      case KEY_DOWN:
      case KEY_CTRL('N'):
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
      default:
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }
   if (result == HANDLED) {
      int size = ScreenManager_size(this->scr);
      for (int i = 1; i < size; i++)
         ScreenManager_remove(this->scr, 1);

      if (selected >= 0 && (size_t)selected < ARRAYSIZE(categoriesPanelPages)) {
         categoriesPanelPages[selected].ctor(this);
      }
   }
   return result;
}

const PanelClass CategoriesPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = CategoriesPanel_delete
   },
   .eventHandler = CategoriesPanel_eventHandler
};

CategoriesPanel* CategoriesPanel_new(ScreenManager* scr, Header* header, Machine* host) {
   CategoriesPanel* this = AllocThis(CategoriesPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(CategoriesFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->scr = scr;
   this->host = host;
   this->header = header;
   Panel_setHeader(super, "Categories");
   for (size_t i = 0; i < ARRAYSIZE(categoriesPanelPages); i++)
      Panel_add(super, (Object*) ListItem_new(categoriesPanelPages[i].name, 0));

   ScreenManager_add(scr, super, 16);
   categoriesPanelPages[0].ctor(this);
   return this;
}
