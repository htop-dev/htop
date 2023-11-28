/*
htop - ScreensPanel.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ScreensPanel.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "AvailableColumnsPanel.h"
#include "CRT.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "ProvideCurses.h"
#include "Settings.h"
#include "XUtils.h"


static void ScreenListItem_delete(Object* cast) {
   ScreenListItem* this = (ScreenListItem*)cast;
   if (this->ss) {
      ScreenSettings_delete(this->ss);
   }
   ListItem_delete(cast);
}

ObjectClass ScreenListItem_class = {
   .extends = Class(ListItem),
   .display = ListItem_display,
   .delete = ScreenListItem_delete,
   .compare = ListItem_compare
};

ScreenListItem* ScreenListItem_new(const char* value, ScreenSettings* ss) {
   ScreenListItem* this = AllocThis(ScreenListItem);
   ListItem_init((ListItem*)this, value, 0);
   this->ss = ss;
   return this;
}

static const char* const ScreensFunctions[] = {"      ", "Rename", "      ", "      ", "New   ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};
static const char* const DynamicFunctions[] = {"      ", "Rename", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static void ScreensPanel_delete(Object* object) {
   Panel* super = (Panel*) object;

   /* do not delete screen settings still in use */
   int n = Panel_size(super);
   for (int i = 0; i < n; i++) {
      ScreenListItem* item = (ScreenListItem*) Panel_get(super, i);
      item->ss = NULL;
   }

   Panel_delete(object);
}

static HandlerResult ScreensPanel_eventHandlerRenaming(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;

   if (ch >= 32 && ch < 127 && ch != '=') {
      if (this->cursor < SCREEN_NAME_LEN - 1) {
         this->buffer[this->cursor] = (char)ch;
         this->cursor++;
         super->selectedLen = strlen(this->buffer);
         Panel_setCursorToSelection(super);
      }

      return HANDLED;
   }

   switch (ch) {
      case 127:
      case KEY_BACKSPACE:
         if (this->cursor > 0) {
            this->cursor--;
            this->buffer[this->cursor] = '\0';
            super->selectedLen = strlen(this->buffer);
            Panel_setCursorToSelection(super);
         }
         break;
      case '\n':
      case '\r':
      case KEY_ENTER: {
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (!item)
            break;
         assert(item == this->renamingItem);
         free(this->saved);
         item->value = xStrdup(this->buffer);
         this->renamingItem = NULL;
         super->cursorOn = false;
         Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
         ScreensPanel_update(super);
         break;
      }
      case 27: { // Esc
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (!item)
            break;
         assert(item == this->renamingItem);
         item->value = this->saved;
         this->renamingItem = NULL;
         super->cursorOn = false;
         Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
         break;
      }
   }

   return HANDLED;
}

static void startRenaming(Panel* super) {
   ScreensPanel* const this = (ScreensPanel*) super;

   ListItem* item = (ListItem*) Panel_getSelected(super);
   if (item == NULL)
      return;
   this->renamingItem = item;
   super->cursorOn = true;
   char* name = item->value;
   this->saved = name;
   strncpy(this->buffer, name, SCREEN_NAME_LEN);
   this->buffer[SCREEN_NAME_LEN] = '\0';
   this->cursor = strlen(this->buffer);
   item->value = this->buffer;
   Panel_setSelectionColor(super, PANEL_EDIT);
   super->selectedLen = strlen(this->buffer);
   Panel_setCursorToSelection(super);
}

static void rebuildSettingsArray(Panel* super, int selected) {
   ScreensPanel* const this = (ScreensPanel*) super;

   int n = Panel_size(super);
   free(this->settings->screens);
   this->settings->screens = xMallocArray(n + 1, sizeof(ScreenSettings*));
   this->settings->screens[n] = NULL;
   for (int i = 0; i < n; i++) {
      ScreenListItem* item = (ScreenListItem*) Panel_get(super, i);
      this->settings->screens[i] = item->ss;
   }
   this->settings->nScreens = n;
   /* ensure selection is in valid range */
   if (selected > n - 1)
      selected = n - 1;
   else if (selected < 0)
      selected = 0;
   this->settings->ssIndex = selected;
   this->settings->ss = this->settings->screens[selected];
}

static void addNewScreen(Panel* super) {
   ScreensPanel* const this = (ScreensPanel*) super;

   const char* name = "New";
   ScreenSettings* ss = Settings_newScreen(this->settings, &(const ScreenDefaults) { .name = name, .columns = "PID Command", .sortKey = "PID" });
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

   switch (ch) {
      case '\n':
      case '\r':
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK: {
         this->moving = !this->moving;
         Panel_setSelectionColor(super, this->moving ? PANEL_SELECTION_FOLLOW : PANEL_SELECTION_FOCUS);
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (item)
            item->moving = this->moving;
         result = HANDLED;
         break;
      }
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END:
         Panel_onKey(super, ch);
         break;
      case KEY_F(2):
      case KEY_CTRL('R'):
         startRenaming(super);
         result = HANDLED;
         break;
      case KEY_F(5):
      case KEY_CTRL('N'):
         if (this->settings->dynamicScreens)
            break;
         addNewScreen(super);
         startRenaming(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      case KEY_UP:
         if (!this->moving) {
            Panel_onKey(super, ch);
            break;
         }
         /* FALLTHRU */
      case KEY_F(7):
      case '[':
      case '-':
         Panel_moveSelectedUp(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      case KEY_DOWN:
         if (!this->moving) {
            Panel_onKey(super, ch);
            break;
         }
         /* FALLTHRU */
      case KEY_F(8):
      case ']':
      case '+':
         Panel_moveSelectedDown(super);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      case KEY_F(9):
      //case KEY_DC:
         if (Panel_size(super) > 1)
            Panel_remove(super, selected);
         shouldRebuildArray = true;
         result = HANDLED;
         break;
      default:
         if (ch < 255 && isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   ScreenListItem* newFocus = (ScreenListItem*) Panel_getSelected(super);
   if (newFocus && oldFocus != newFocus) {
      Hashtable* dynamicColumns = this->settings->dynamicColumns;
      ColumnsPanel_fill(this->columns, newFocus->ss, dynamicColumns);
      AvailableColumnsPanel_fill(this->availableColumns, newFocus->ss->dynamic, dynamicColumns);
      result = HANDLED;
   }

   if (shouldRebuildArray)
      rebuildSettingsArray(super, selected);
   if (result == HANDLED)
      ScreensPanel_update(super);

   return result;
}

static HandlerResult ScreensPanel_eventHandler(Panel* super, int ch) {
   ScreensPanel* const this = (ScreensPanel*) super;

   if (this->renamingItem) {
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
   Hashtable* columns = settings->dynamicColumns;
   FunctionBar* fuBar = FunctionBar_new(settings->dynamicScreens ? DynamicFunctions : ScreensFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->columns = ColumnsPanel_new(settings->screens[0], columns, &(settings->changed));
   this->availableColumns = AvailableColumnsPanel_new((Panel*) this->columns, columns);
   this->moving = false;
   this->renamingItem = NULL;
   super->cursorOn = false;
   this->cursor = 0;
   Panel_setHeader(super, "Screens");

   for (unsigned int i = 0; i < settings->nScreens; i++) {
      ScreenSettings* ss = settings->screens[i];
      char* name = ss->heading;
      Panel_add(super, (Object*) ScreenListItem_new(name, ss));
   }
   return this;
}

void ScreensPanel_update(Panel* super) {
   ScreensPanel* this = (ScreensPanel*) super;
   int size = Panel_size(super);
   this->settings->changed = true;
   this->settings->lastUpdate++;
   this->settings->screens = xReallocArray(this->settings->screens, size + 1, sizeof(ScreenSettings*));
   for (int i = 0; i < size; i++) {
      ScreenListItem* item = (ScreenListItem*) Panel_get(super, i);
      ScreenSettings* ss = item->ss;
      free_and_xStrdup(&ss->heading, ((ListItem*) item)->value);
      this->settings->screens[i] = ss;
   }
   this->settings->screens[size] = NULL;
}
