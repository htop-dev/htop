/*
htop - ColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ColumnsPanel.h"
#include "Platform.h"

#include "StringUtils.h"
#include "ListItem.h"
#include "CRT.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>


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

   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
      {
         if (selected < size - 1) {
            this->moving = !(this->moving);
            Panel_setSelectionColor(super, this->moving ? CRT_colors[PANEL_SELECTION_FOLLOW] : CRT_colors[PANEL_SELECTION_FOCUS]);
            ((ListItem*)Panel_getSelected(super))->moving = this->moving;
            result = HANDLED;
         }
         break;
      }
      case KEY_UP:
      {
         if (!this->moving) {
            break;
         }
      }
         /* else fallthrough */
      case KEY_F(7):
      case '[':
      case '-':
      {
         if (selected < size - 1)
            Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_DOWN:
      {
         if (!this->moving) {
            break;
         }
      }
         /* else fallthrough */
      case KEY_F(8):
      case ']':
      case '+':
      {
         if (selected < size - 2)
            Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      case KEY_DC:
      {
         if (selected < size - 1) {
            Panel_remove(super, selected);
         }
         result = HANDLED;
         break;
      }
      default:
      {
         if (ch < 255 && isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
      }
   }
   if (result == HANDLED)
      ColumnsPanel_update(super);
   return result;
}

PanelClass ColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ColumnsPanel_delete
   },
   .eventHandler = ColumnsPanel_eventHandler
};

ColumnsPanel* ColumnsPanel_new(Settings* settings) {
   ColumnsPanel* this = AllocThis(ColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->moving = false;
   Panel_setHeader(super, "Active Columns");

   ProcessField* fields = this->settings->fields;
   for (; *fields; fields++) {
      if (Process_fields[*fields].name) {
         Panel_add(super, (Object*) ListItem_new(Process_fields[*fields].name, *fields));
      }
   }
   return this;
}

int ColumnsPanel_fieldNameToIndex(const char* name) {
   for (int j = 1; j <= Platform_numberOfFields; j++) {
      if (String_eq(name, Process_fields[j].name)) {
         return j;
      }
   }
   return -1;
}

void ColumnsPanel_update(Panel* super) {
   ColumnsPanel* this = (ColumnsPanel*) super;
   int size = Panel_size(super);
   this->settings->changed = true;
   this->settings->fields = xRealloc(this->settings->fields, sizeof(ProcessField) * (size+1));
   this->settings->flags = 0;
   for (int i = 0; i < size; i++) {
      int key = ((ListItem*) Panel_get(super, i))->key;
      this->settings->fields[i] = key;
      this->settings->flags |= Process_fields[key].flags;
   }
   this->settings->fields[size] = 0;
}
