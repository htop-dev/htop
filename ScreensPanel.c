/*
htop - ScreensPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreensPanel.h"
#include "Platform.h"

#include "StringUtils.h"
#include "ListItem.h"
#include "CRT.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Settings.h"

#ifndef SCREEN_NAME_LEN
#define SCREEN_NAME_LEN 20
#endif

typedef struct ScreensPanel_ {
   Panel super;

   Settings* settings;
   char buffer[SCREEN_NAME_LEN + 1];
   char* saved;
   int cursor;
   bool moving;
   bool renaming;
} ScreensPanel;

}*/

static const char* const ScreensFunctions[] = {"      ", "Rename", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

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
   ScreensPanel_update(super);
   return HANDLED;
}

void startRenaming(Panel* super) {
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

static HandlerResult ScreensPanel_eventHandlerNormal(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;
   
   int selected = Panel_getSelectedIndex(super);
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
      case KEY_F(2):
      case 0x12: /* Ctrl+R */
      {
         startRenaming(super);
         result = HANDLED;
         break;
      }
      case KEY_F(5):
      case 0x0e: /* Ctrl+N */
      {
         ListItem* item = ListItem_new("", 0);
         int idx = Panel_getSelectedIndex(super);
         Panel_insert(super, idx + 1, (Object*) item);
         Panel_setSelected(super, idx + 1);
         startRenaming(super);
         result = HANDLED;
         break;
      }
      case KEY_UP:
      {
         if (!this->moving) {
            break;
         }
         /* else fallthrough */
      }
      case KEY_F(7):
      case '[':
      case '-':
      {
         Panel_moveSelectedUp(super);
         result = HANDLED;
         break;
      }
      case KEY_DOWN:
      {
         if (!this->moving) {
            break;
         }
         /* else fallthrough */
      }
      case KEY_F(8):
      case ']':
      case '+':
      {
         Panel_moveSelectedDown(super);
         result = HANDLED;
         break;
      }
      case KEY_F(9):
      //case KEY_DC:
      {
         Panel_remove(super, selected);
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
   this->moving = false;
   this->renaming = false;
   super->cursorOn = false;
   this->cursor = 0;
   Panel_setHeader(super, "Screens");

   char** screens = this->settings->screens;
   for (; *screens; screens++) {
      char* name = *screens;
      Panel_add(super, (Object*) ListItem_new(name, 0));
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
      this->settings->screens[i] = xStrdup(name);
   }
   this->settings->screens[size] = NULL;
}
