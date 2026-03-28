/*
htop - ScreensPanel.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2026 htop dev team
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
#include "LineEditor.h"
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
static const char* const ScreensRenamingFunctions[] = {"      ", "Cancel", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};
static FunctionBar* Screens_renamingBar = NULL;

static void rebuildSettingsArray(Panel* super, int selected);

void ScreensPanel_cleanup(void) {
   if (Screens_renamingBar) {
      FunctionBar_delete(Screens_renamingBar);
      Screens_renamingBar = NULL;
   }
}

static void ScreensPanel_cancelMoving(ScreensPanel* this) {
   Panel* super = &this->super;
   for (int i = 0; i < Panel_size(super); i++) {
      ListItem* item = (ListItem*) Panel_get(super, i);
      if (item)
         item->moving = false;
   }
   this->moving = false;
   Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
}

static void ScreensPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ScreensPanel* const this = (ScreensPanel*) super;

   /* cancel any pending edit action */
   if (this->renamingItem) {
      ListItem* item = (ListItem*) Panel_getSelected(super);
      assert(item == this->renamingItem);

      if (item)
         item->value = this->saved;
      this->renamingItem = NULL;
      super->cursorOn = false;

      Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
   }

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

   switch (ch) {
      case EVENT_SET_SELECTED: {
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (item != this->renamingItem)
            goto renameFinish;
         break;
      }
      case EVENT_PANEL_LOST_FOCUS:
         goto renameFinish;
      case '\n':
      case '\r':
      case KEY_ENTER:
      case KEY_F(10): {
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (!item)
            break;
         assert(item == this->renamingItem);
renameFinish:
         if (!this->renamingItem)
            break;
         free(this->saved);
         this->renamingItem->value = xStrdup(LineEditor_getText(&this->editor));
         this->renamingItem = NULL;
         this->renamingNewItem = false;
         super->cursorOn = false;
         Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
         Panel_setDefaultBar(super);
         ScreensPanel_update(super);
         break;
      }
      case 27: // Esc
      case KEY_F(2): {
         ListItem* item = (ListItem*) Panel_getSelected(super);
         if (!item)
            break;
         assert(item == this->renamingItem);

         // Restore item->value to the heap-allocated saved string.
         // This is safe for both cases: existing items keep their name,
         // and new items need this before deletion to avoid freeing the stack buffer.
         item->value = this->saved;

         if (this->renamingNewItem) {
            // If canceling a newly created item, delete it
            Panel_remove(super, Panel_getSelectedIndex(super));
            // Rebuild with the updated selection after removal
            rebuildSettingsArray(super, Panel_getSelectedIndex(super));
         }

         this->renamingNewItem = false;
         this->renamingItem = NULL;
         super->cursorOn = false;
         Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
         Panel_setDefaultBar(super);
         break;
      }
      default: {
         /* Delegate editing keys (printable chars, cursor movement, etc.) to LineEditor.
            Exclude '=' to not break the htop config format. */
         if (ch == '=')
            break;
         LineEditor_handleKey(&this->editor, ch);
         super->selectedLen = LineEditor_getCursor(&this->editor);
         Panel_setCursorToSelection(super);
         /* Keep item->value pointing to the display buffer */
         if (this->renamingItem)
            this->renamingItem->value = LineEditor_getText(&this->editor);
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
   /* Initialise the line editor with the current name (limited to SCREEN_NAME_LEN - 1 chars) */
   LineEditor_initWithMax(&this->editor, SCREEN_NAME_LEN - 1);
   LineEditor_setText(&this->editor, name);
   /* Point the item's value at the editor buffer so Panel draws it live */
   item->value = LineEditor_getText(&this->editor);
   Panel_setSelectionColor(super, PANEL_EDIT);
   super->selectedLen = LineEditor_getCursor(&this->editor);
   Panel_setCursorToSelection(super);
   super->currentBar = Screens_renamingBar;
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

   const void* oldFocus = Panel_get(super, super->prevSelected);
   bool shouldRebuildArray = false;
   HandlerResult result = IGNORED;

   switch (ch) {
      case '\n':
      case '\r':
      case KEY_ENTER:
         if (this->moving) {
            ScreensPanel_cancelMoving(this);
         } else {
            this->moving = true;
            Panel_setSelectionColor(super, PANEL_SELECTION_FOLLOW);
            ListItem* item = (ListItem*) Panel_getSelected(super);
            if (item)
               item->moving = true;
         }
         result = HANDLED;
         break;
      case KEY_MOUSE:
         if (this->moving) {
            /* Single click while in move mode: cancel move mode */
            ScreensPanel_cancelMoving(this);
            result = HANDLED;
         }
         /* else: just select the item, do not enter move mode */
         break;
      case KEY_RECLICK:
         /* Double click: start renaming */
         this->renamingNewItem = false;
         startRenaming(super);
         result = HANDLED;
         break;
      case EVENT_SET_SELECTED:
         if (this->moving)
            ScreensPanel_cancelMoving(this);
         result = HANDLED;
         break;
      case EVENT_PANEL_LOST_FOCUS:
         if (this->moving)
            ScreensPanel_cancelMoving(this);
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
         this->renamingNewItem = false;
         startRenaming(super);
         result = HANDLED;
         break;
      case KEY_F(5):
      case KEY_CTRL('N'):
         if (this->settings->dynamicScreens)
            break;
         addNewScreen(super);
         this->renamingNewItem = true;
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
      case KEY_DC:
      case KEY_DEL_MAC:
         if (Panel_size(super) > 1)
            Panel_remove(super, super->selected);
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

   super->prevSelected = super->selected;

   if (shouldRebuildArray)
      rebuildSettingsArray(super, super->selected);

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
   Panel* super = &this->super;

   FunctionBar* fuBar = FunctionBar_new(settings->dynamicScreens ? DynamicFunctions : ScreensFunctions, NULL, NULL);
   if (!Screens_renamingBar) {
      Screens_renamingBar = FunctionBar_new(ScreensRenamingFunctions, NULL, NULL);
   }
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   Hashtable* columns = settings->dynamicColumns;

   this->settings = settings;
   this->columns = ColumnsPanel_new(settings->screens[0], columns, &(settings->changed));
   this->availableColumns = AvailableColumnsPanel_new((Panel*) this->columns, columns);
   this->moving = false;
   this->renamingItem = NULL;
   this->renamingNewItem = false;
   this->saved = NULL;
   super->cursorOn = false;
   LineEditor_initWithMax(&this->editor, SCREEN_NAME_LEN - 1);
   Panel_setHeader(super, "Screens");

   for (unsigned int i = 0; i < settings->nScreens; i++) {
      ScreenSettings* ss = settings->screens[i];
      char* name = ss->heading;
      Panel_add(super, (Object*) ScreenListItem_new(name, ss));
   }

   super->prevSelected = super->selected;

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
