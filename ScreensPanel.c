/*
htop - ScreensPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreensPanel.h"
#include "Platform.h"

#include "StringUtils.h"
#include "CRT.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/*{
#include "Panel.h"
#include "ScreenManager.h"
#include "ColumnsPanel.h"
#include "Settings.h"
#include "ListItem.h"

#ifndef SCREEN_NAME_LEN
#define SCREEN_NAME_LEN 20
#endif

typedef struct ScreensPanel_ {
   Panel super;
   
   ScreenManager* scr;
   Settings* settings;
   ColumnsPanel* columns;
   char buffer[SCREEN_NAME_LEN + 1];
   char* saved;
   int cursor;
   bool moving;
   bool renaming;
} ScreensPanel;

typedef struct ScreenListItem_ {
   ListItem super;
   ScreenSettings* ss;
} ScreenListItem;

}*/

ObjectClass ScreenListItem_class = {
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};

ScreenListItem* ScreenListItem_new(const char* value, ScreenSettings* ss) {
   ScreenListItem* this = AllocThis(ScreenListItem);
   ListItem_init((ListItem*)this, value, 0);
   this->ss = ss;
   return this;
}

static const char* const ScreensFunctions[] = {"      ", "Rename", "      ", "      ", "New   ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static void ScreensPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ScreensPanel* this = (ScreensPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ScreensPanel_eventHandlerRenaming(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;

   //ListItem* item = (ListItem*)Panel_getSelected(super);
   if (ch >= 32 && ch < 127 && ch != 61 && ch != 22) {
      if (this->cursor < SCREEN_NAME_LEN - 1) {
         this->buffer[this->cursor] = ch;
         this->cursor++;
         super->selectedLen = strlen(this->buffer);
         Panel_setCursorToSelection(super);
      }
   } else {
      switch(ch) {
         case 127:
         case KEY_BACKSPACE:
         {
            if (this->cursor > 0) {
               this->cursor--;
               this->buffer[this->cursor] = '\0';
               super->selectedLen = strlen(this->buffer);
               Panel_setCursorToSelection(super);
            }
            break;
         }
         case 0x0a:
         case 0x0d:
         case KEY_ENTER:
         {
            ListItem* item = (ListItem*) Panel_getSelected(super);
            free(this->saved);
            item->value = xStrdup(this->buffer);
            this->renaming = false;
            super->cursorOn = false;
            Panel_setSelectionColor(super, CRT_colors[PANEL_SELECTION_FOCUS]);
            ScreensPanel_update(super);
            break;
         }
         case 27: // Esc
         {
            ListItem* item = (ListItem*) Panel_getSelected(super);
            item->value = this->saved;
            this->renaming = false;
            super->cursorOn = false;
            Panel_setSelectionColor(super, CRT_colors[PANEL_SELECTION_FOCUS]);
            break;
         }
      }
   }
   return HANDLED;
}

static void startRenaming(Panel* super) {
   ScreensPanel* const this = (ScreensPanel*) super;

   ListItem* item = (ListItem*) Panel_getSelected(super);
   this->renaming = true;
   super->cursorOn = true;
   char* name = item->value;
   this->saved = name;
   strncpy(this->buffer, name, SCREEN_NAME_LEN);
   this->buffer[SCREEN_NAME_LEN] = '\0';
   this->cursor = strlen(this->buffer);
   item->value = this->buffer;
   Panel_setSelectionColor(super, CRT_colors[PANEL_EDIT]);
   super->selectedLen = strlen(this->buffer);
   Panel_setCursorToSelection(super);
}

static void rebuildSettingsArray(Panel* super) {
   ScreensPanel* const this = (ScreensPanel*) super;

   int n = Panel_size(super);
   free(this->settings->screens);
   this->settings->screens = xMalloc(sizeof(ScreenSettings*) * (n + 1));
   this->settings->screens[n] = NULL;
   for (int i = 0; i < n; i++) {
      ScreenListItem* item = (ScreenListItem*) Panel_get(super, i);
      this->settings->screens[i] = item->ss;
   }
}

static void addNewScreen(Panel* super) {
   ScreensPanel* const this = (ScreensPanel*) super;

   char* name = "New";
   ScreenSettings* ss = Settings_newScreen(this->settings, name, "PID Command");
   ScreenListItem* item = ScreenListItem_new(name, ss);
   int idx = Panel_getSelectedIndex(super);
   Panel_insert(super, idx + 1, (Object*) item);
   Panel_setSelected(super, idx + 1);
}

static HandlerResult ScreensPanel_eventHandlerNormal(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;
   
   int selected = Panel_getSelectedIndex(super);
   ScreenListItem* oldFocus = (ScreenListItem*) Panel_getSelected(super);
   bool shouldRebuildArray = false;
   HandlerResult result = IGNORED;
   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
      {
         this->moving = !(this->moving);
         Panel_setSelectionColor(super, this->moving ? CRT_colors[PANEL_SELECTION_FOLLOW] : CRT_colors[PANEL_SELECTION_FOCUS]);
         ((ListItem*)Panel_getSelected(super))->moving = this->moving;
         result = HANDLED;
         break;
      }
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END: {
         Panel_onKey(super, ch);
         break;
      }
      case KEY_F(2):
      case KEY_CTRL('R'):
      {
         startRenaming(super);
         result = HANDLED;
         break;
      }
      case KEY_F(5):
      case KEY_CTRL('N'):
      {
         addNewScreen(super);
         startRenaming(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      }
      case KEY_UP:
      {
         if (!this->moving) {
            Panel_onKey(super, ch);
            break;
         }
         /* else fallthrough */
      }
      case KEY_F(7):
      case '[':
      case '-':
      {
         Panel_moveSelectedUp(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      }
      case KEY_DOWN:
      {
         if (!this->moving) {
            Panel_onKey(super, ch);
            break;
         }
         /* else fallthrough */
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
         Panel_moveSelectedDown(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      //case KEY_DC:
      {
         if (Panel_size(super) > 1) {
            Panel_remove(super, selected);
         }
         shouldRebuildArray = true;
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
   ScreenListItem* newFocus = (ScreenListItem*) Panel_getSelected(super);
   if (oldFocus != newFocus) {
      ColumnsPanel_fill(this->columns, newFocus->ss);
      result = HANDLED;
   }
   if (shouldRebuildArray) {
      rebuildSettingsArray(super);
   }
   if (result == HANDLED)
      ScreensPanel_update(super);
   return result;
}
   
static HandlerResult ScreensPanel_eventHandler(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;

   if (this->renaming) {
      return ScreensPanel_eventHandlerRenaming(super, ch);
   } else {
      return ScreensPanel_eventHandlerNormal(super, ch);
   }
}

PanelClass ScreensPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ScreensPanel_delete
   },
   .eventHandler = ScreensPanel_eventHandler
};

ScreensPanel* ScreensPanel_new(Settings* settings) {
   ScreensPanel* this = AllocThis(ScreensPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ScreensFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->columns = ColumnsPanel_new(settings->screens[0], &(settings->changed));
   this->moving = false;
   this->renaming = false;
   super->cursorOn = false;
   this->cursor = 0;
   Panel_setHeader(super, "Screens");

   for (unsigned int i = 0; i < settings->nScreens; i++) {
      ScreenSettings* ss = settings->screens[i];
      char* name = ss->name;
      Panel_add(super, (Object*) ScreenListItem_new(name, ss));
   }
   return this;
}

void ScreensPanel_update(Panel* super) {
   ScreensPanel* this = (ScreensPanel*) super;
   int size = Panel_size(super);
   this->settings->changed = true;
   this->settings->screens = xRealloc(this->settings->screens, sizeof(char*) * (size+1));
   for (int i = 0; i < size; i++) {
      char* name = ((ListItem*) Panel_get(super, i))->value;
      this->settings->screens[i]->name = xStrdup(name);
   }
   this->settings->screens[size] = NULL;
}
