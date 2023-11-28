/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "AvailableColumnsPanel.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ColumnsPanel.h"
#include "DynamicColumn.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "ListItem.h"
#include "Object.h"
#include "Platform.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RowField.h"
#include "XUtils.h"


static const char* const AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static void AvailableColumnsPanel_insert(AvailableColumnsPanel* this, int at, int key) {
   const char* name;
   if (key >= ROW_DYNAMIC_FIELDS)
      name = DynamicColumn_name(key);
   else
      name = Process_fields[key].name;
   Panel_insert(this->columns, at, (Object*) ListItem_new(name, key));
}

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   HandlerResult result = IGNORED;

   switch (ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5): {
         const ListItem* selected = (ListItem*) Panel_getSelected(super);
         if (!selected)
            break;

         int at = Panel_getSelectedIndex(this->columns);
         AvailableColumnsPanel_insert(this, at, selected->key);
         Panel_setSelected(this->columns, at + 1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      }
      default:
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         break;
   }
   return result;
}

const PanelClass AvailableColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableColumnsPanel_delete
   },
   .eventHandler = AvailableColumnsPanel_eventHandler
};

static void AvailableColumnsPanel_addDynamicColumn(ht_key_t key, void* value, void* data) {
   const DynamicColumn* column = (const DynamicColumn*) value;
   if (column->table) /* DynamicScreen, handled differently */
      return;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) data;
   const char* title = column->heading ? column->heading : column->name;
   const char* text = column->description ? column->description : column->caption;
   char description[256];
   if (text)
      xSnprintf(description, sizeof(description), "%s - %s", title, text);
   else
      xSnprintf(description, sizeof(description), "%s", title);
   Panel_add(&this->super, (Object*) ListItem_new(description, key));
}

// Handle DynamicColumns entries in the AvailableColumnsPanel
static void AvailableColumnsPanel_addDynamicColumns(AvailableColumnsPanel* this, Hashtable* dynamicColumns) {
   assert(dynamicColumns);
   Hashtable_foreach(dynamicColumns, AvailableColumnsPanel_addDynamicColumn, this);
}

// Handle remaining Platform Meter entries in the AvailableColumnsPanel
static void AvailableColumnsPanel_addPlatformColumns(AvailableColumnsPanel* this) {
   for (int i = 1; i < LAST_PROCESSFIELD; i++) {
      if (i != COMM && Process_fields[i].description) {
         char description[256];
         xSnprintf(description, sizeof(description), "%s - %s", Process_fields[i].name, Process_fields[i].description);
         Panel_add(&this->super, (Object*) ListItem_new(description, i));
      }
   }
}

// Handle DynamicColumns entries associated with DynamicScreens
static void AvailableColumnsPanel_addDynamicScreens(AvailableColumnsPanel* this, const char* screen) {
   Platform_addDynamicScreenAvailableColumns(&this->super, screen);
}

void AvailableColumnsPanel_fill(AvailableColumnsPanel* this, const char* dynamicScreen, Hashtable* dynamicColumns) {
   Panel* super = (Panel*) this;
   Panel_prune(super);
   if (dynamicScreen) {
      AvailableColumnsPanel_addDynamicScreens(this, dynamicScreen);
   } else {
      AvailableColumnsPanel_addPlatformColumns(this);
      AvailableColumnsPanel_addDynamicColumns(this, dynamicColumns);
   }
}

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns, Hashtable* dynamicColumns) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(AvailableColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);
   Panel_setHeader(super, "Available Columns");

   this->columns = columns;
   AvailableColumnsPanel_fill(this, NULL, dynamicColumns);

   return this;
}
