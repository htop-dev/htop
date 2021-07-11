/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableColumnsPanel.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ColumnsPanel.h"
#include "DynamicColumn.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "ListItem.h"
#include "Object.h"
#include "Process.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "XUtils.h"

typedef struct {
   Panel* super;
   unsigned int id;
   unsigned int offset;
} DynamicIterator;


static const char* const AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}


/*
void AvailableColumnsPanel_insertDynamicColumnName(AvailableColumnsPanel* this, int at, int key, const ProcessList* pl) {
   int param = abs(key - LAST_STATIC_PROCESSFIELD);
   const DynamicColumn* column = Hashtable_get(pl->dynamicColumns, param);
   //Panel_insert(this->columns, at, (Object*) ListItem_new("lol", 1));
}
*/

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   HandlerResult result = IGNORED;

   switch(ch) {
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
      {
         const ListItem* selected = (ListItem*) Panel_getSelected(super);
         if (!selected)
            break;

         int key = selected->key;
         int at = Panel_getSelectedIndex(this->columns);
         if( key > LAST_STATIC_PROCESSFIELD) {
            // FIXME get caption of the column from pl->dynamicColumns
            //AvailableColumnsPanel_insertDynamicColumnName(this, at, key, this->pl);
            Panel_insert(this->columns, at, (Object*) ListItem_new("Dynamic", key));
         } else {
            Panel_insert(this->columns, at, (Object*) ListItem_new(Process_fields[key].name, key));
         }
         Panel_setSelected(this->columns, at+1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      }
      default:
      {
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         break;
      }
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


static void AvailableColumnsPanel_addDynamicColumn(ATTR_UNUSED ht_key_t key, void* value, void* data) {
   const DynamicColumn* column = (const DynamicColumn*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   int fields = LAST_STATIC_PROCESSFIELD+iter->id;
   char description[256];
   xSnprintf(description, sizeof(description), "%s - %s", column->caption, column->description);
   Panel_add(iter->super, (Object*) ListItem_new(description, fields));
   iter->id++;
}

// Handle DynamicColumns entries in the AvailableColumnsPanel
static void AvailableColumnsPanel_addDynamicColumns(Panel* super, const ProcessList* pl) {
   DynamicIterator iter = { .super = super, .id = 1, .offset = 0 };
   assert(pl->dynamicMeters != NULL);
   Hashtable_foreach(pl->dynamicColumns, AvailableColumnsPanel_addDynamicColumn, &iter);
}


// Handle remaining Platform Meter entries in the AvailableColumnsPanel
static void AvailableColumnsPanel_addPlatformColumn(Panel* super) {
   for (int i = 1; i < LAST_STATIC_PROCESSFIELD; i++) {
      if (i != COMM && Process_fields[i].description) {
         char description[256];
         xSnprintf(description, sizeof(description), "%s - %s", Process_fields[i].name, Process_fields[i].description);
         Panel_add(super, (Object*) ListItem_new(description, i));
      }
   }
}

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns, const ProcessList* pl) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(AvailableColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   Panel_setHeader(super, "Available Columns");

   AvailableColumnsPanel_addPlatformColumn(super);

   AvailableColumnsPanel_addDynamicColumns(super, pl);

   this->columns = columns;
   return this;
}
