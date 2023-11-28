/*
htop - ColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ColumnsPanel.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "ListItem.h"
#include "Object.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RowField.h"
#include "XUtils.h"


static const char* const ColumnsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static void ColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ColumnsPanel* this = (ColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ColumnsPanel_eventHandler(Panel* super, int ch) {
   ColumnsPanel* const this = (ColumnsPanel*) super;

   int selected = Panel_getSelectedIndex(super);
   HandlerResult result = IGNORED;
   int size = Panel_size(super);

   switch (ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
         if (selected < size - 1) {
            this->moving = !(this->moving);
            Panel_setSelectionColor(super, this->moving ? PANEL_SELECTION_FOLLOW : PANEL_SELECTION_FOCUS);
            ListItem* selectedItem = (ListItem*) Panel_getSelected(super);
            if (selectedItem)
               selectedItem->moving = this->moving;
            result = HANDLED;
         }
         break;
      case KEY_UP:
         if (!this->moving)
            break;
         /* else fallthrough */
      case KEY_F(7):
      case '[':
      case '-':
         if (selected < size - 1)
            Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      case KEY_DOWN:
         if (!this->moving)
            break;
         /* else fallthrough */
      case KEY_F(8):
      case ']':
      case '+':
         if (selected < size - 2)
            Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      case KEY_F(9):
      case KEY_DC:
         if (selected < size - 1)
            Panel_remove(super, selected);
         result = HANDLED;
         break;
      default:
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   if (result == HANDLED)
      ColumnsPanel_update(super);

   return result;
}

const PanelClass ColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ColumnsPanel_delete
   },
   .eventHandler = ColumnsPanel_eventHandler
};

static void ColumnsPanel_add(Panel* super, unsigned int key, Hashtable* columns) {
   const char* name;
   if (key < LAST_PROCESSFIELD) {
      name = Process_fields[key].name;
   } else {
      const DynamicColumn* column = Hashtable_get(columns, key);
      assert(column);
      if (!column) {
         name = NULL;
      } else {
         /* heading preferred here but name is always available */
         name = column->heading ? column->heading : column->name;
      }
   }
   if (name == NULL)
      name = "- ";
   Panel_add(super, (Object*) ListItem_new(name, key));
}

void ColumnsPanel_fill(ColumnsPanel* this, ScreenSettings* ss, Hashtable* columns) {
   Panel* super = (Panel*) this;
   Panel_prune(super);
   for (const RowField* fields = ss->fields; *fields; fields++)
      ColumnsPanel_add(super, *fields, columns);
   this->ss = ss;
}

ColumnsPanel* ColumnsPanel_new(ScreenSettings* ss, Hashtable* columns, bool* changed) {
   ColumnsPanel* this = AllocThis(ColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->ss = ss;
   this->changed = changed;
   this->moving = false;
   Panel_setHeader(super, "Active Columns");

   ColumnsPanel_fill(this, ss, columns);

   return this;
}

void ColumnsPanel_update(Panel* super) {
   ColumnsPanel* this = (ColumnsPanel*) super;
   int size = Panel_size(super);
   *(this->changed) = true;
   this->ss->fields = xRealloc(this->ss->fields, sizeof(ProcessField) * (size + 1));
   this->ss->flags = 0;
   for (int i = 0; i < size; i++) {
      int key = ((ListItem*) Panel_get(super, i))->key;
      this->ss->fields[i] = key;
      if (key < LAST_PROCESSFIELD)
         this->ss->flags |= Process_fields[key].flags;
   }
   this->ss->fields[size] = 0;
}
