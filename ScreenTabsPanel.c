/*
htop - ScreenTabsPanel.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ScreenTabsPanel.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "Macros.h"
#include "ProvideCurses.h"
#include "Settings.h"
#include "XUtils.h"


static HandlerResult ScreenNamesPanel_eventHandlerNormal(Panel* super, int ch);

ObjectClass ScreenTabListItem_class = {
   .extends = Class(ListItem),
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};

static void ScreenNamesPanel_fill(ScreenNamesPanel* this, DynamicScreen* ds) {
   const Settings* settings = this->settings;
   Panel* super = (Panel*) this;
   Panel_prune(super);

   for (unsigned int i = 0; i < settings->nScreens; i++) {
      const ScreenSettings* ss = settings->screens[i];

      if (ds == NULL) {
         if (ss->dynamic != NULL)
            continue;
         /* built-in (processes, not dynamic) - e.g. Main or I/O */
      } else {
         if (ss->dynamic == NULL)
            continue;
         if (!String_eq(ds->name, ss->dynamic))
            continue;
         /* matching dynamic screen found, add it into the Panel */
      }
      Panel_add(super, (Object*) ListItem_new(ss->heading, i));
   }

   this->ds = ds;
}

static void ScreenTabsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ScreenTabsPanel* this = (ScreenTabsPanel*) object;

   Panel_done(super);
   free(this);
}

static HandlerResult ScreenTabsPanel_eventHandler(Panel* super, int ch) {
   ScreenTabsPanel* const this = (ScreenTabsPanel* const) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_F(5):
      case KEY_CTRL('N'):
         /* pass onto the Names panel for creating new screen */
         return ScreenNamesPanel_eventHandlerNormal(&this->names->super, ch);
      case KEY_UP:
      case KEY_DOWN:
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
         if (ch < 255 && isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   if (result == HANDLED) {
      ScreenTabListItem* focus = (ScreenTabListItem*) Panel_getSelected(super);
      if (focus) {
         ScreenNamesPanel_fill(this->names, focus->ds);
      }
   }

   return result;
}

PanelClass ScreenTabsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ScreenTabsPanel_delete,
   },
   .eventHandler = ScreenTabsPanel_eventHandler
};

static ScreenTabListItem* ScreenTabListItem_new(const char* value, DynamicScreen* ds) {
   ScreenTabListItem* this = AllocThis(ScreenTabListItem);
   ListItem_init((ListItem*)this, value, 0);
   this->ds = ds;
   return this;
}

static void addDynamicScreen(ATTR_UNUSED ht_key_t key, void* value, void* userdata) {
   DynamicScreen* screen = (DynamicScreen*) value;
   Panel* super = (Panel*) userdata;
   const char* name = screen->heading ? screen->heading : screen->name;

   Panel_add(super, (Object*) ScreenTabListItem_new(name, screen));
}

static const char* const ScreenTabsFunctions[] = {"      ", "      ", "      ", "      ", "New   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

ScreenTabsPanel* ScreenTabsPanel_new(Settings* settings) {
   ScreenTabsPanel* this = AllocThis(ScreenTabsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ScreenTabsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->names = ScreenNamesPanel_new(settings);
   super->cursorOn = false;
   this->cursor = 0;
   Panel_setHeader(super, "Screen tabs");

   assert(settings->dynamicScreens != NULL);
   Panel_add(super, (Object*) ScreenTabListItem_new("Processes", NULL));
   Hashtable_foreach(settings->dynamicScreens, addDynamicScreen, super);

   return this;
}

// -------------

ObjectClass ScreenNameListItem_class = {
   .extends = Class(ListItem),
   .display = ListItem_display,
   .delete = ListItem_delete,
   .compare = ListItem_compare
};

ScreenNameListItem* ScreenNameListItem_new(const char* value, ScreenSettings* ss) {
   ScreenNameListItem* this = AllocThis(ScreenNameListItem);
   ListItem_init((ListItem*)this, value, 0);
   this->ss = ss;
   return this;
}

static const char* const ScreenNamesFunctions[] = {"      ", "      ", "      ", "      ", "New   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void ScreenNamesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ScreenNamesPanel* this = (ScreenNamesPanel*) object;

   /* do not delete screen settings still in use */
   int n = Panel_size(super);
   for (int i = 0; i < n; i++) {
      ScreenNameListItem* item = (ScreenNameListItem*) Panel_get(super, i);
      item->ss = NULL;
   }

   /* during renaming the ListItem's value points to our static buffer */
   if (this->renamingItem)
      this->renamingItem->value = this->saved;

   Panel_done(super);
   free(this);
}

static void renameScreenSettings(ScreenNamesPanel* this, const ListItem* item) {
   const ScreenNameListItem* nameItem = (const ScreenNameListItem*) item;

   ScreenSettings* ss = nameItem->ss;
   free_and_xStrdup(&ss->heading, item->value);

   Settings* settings = this->settings;
   settings->changed = true;
   settings->lastUpdate++;
}

static HandlerResult ScreenNamesPanel_eventHandlerRenaming(Panel* super, int ch) {
   ScreenNamesPanel* const this = (ScreenNamesPanel*) super;

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
         renameScreenSettings(this, item);
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
   ScreenNamesPanel* const this = (ScreenNamesPanel*) super;

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

static void addNewScreen(Panel* super, DynamicScreen* ds) {
   ScreenNamesPanel* const this = (ScreenNamesPanel*) super;
   const char* name = "New";
   ScreenSettings* ss = (ds != NULL) ? Settings_newDynamicScreen(this->settings, name, ds, NULL) : Settings_newScreen(this->settings, &(const ScreenDefaults) { .name = name, .columns = "PID Command", .sortKey = "PID" });
   ScreenNameListItem* item = ScreenNameListItem_new(name, ss);
   int idx = Panel_getSelectedIndex(super);
   Panel_insert(super, idx + 1, (Object*) item);
   Panel_setSelected(super, idx + 1);
}

static HandlerResult ScreenNamesPanel_eventHandlerNormal(Panel* super, int ch) {
   ScreenNamesPanel* const this = (ScreenNamesPanel*) super;
   ScreenNameListItem* oldFocus = (ScreenNameListItem*) Panel_getSelected(super);
   HandlerResult result = IGNORED;

   switch (ch) {
      case '\n':
      case '\r':
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
         Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
         result = HANDLED;
         break;
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END:
         Panel_onKey(super, ch);
         break;
      case KEY_F(5):
      case KEY_CTRL('N'):
         addNewScreen(super, this->ds);
         startRenaming(super);
         result = HANDLED;
         break;
      default:
         if (ch < 255 && isalpha(ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   ScreenNameListItem* newFocus = (ScreenNameListItem*) Panel_getSelected(super);
   if (newFocus && oldFocus != newFocus)
      result = HANDLED;

   return result;
}

static HandlerResult ScreenNamesPanel_eventHandler(Panel* super, int ch) {
   ScreenNamesPanel* const this = (ScreenNamesPanel*) super;

   if (!this->renamingItem)
      return ScreenNamesPanel_eventHandlerNormal(super, ch);
   return ScreenNamesPanel_eventHandlerRenaming(super, ch);
}

PanelClass ScreenNamesPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ScreenNamesPanel_delete
   },
   .eventHandler = ScreenNamesPanel_eventHandler
};

ScreenNamesPanel* ScreenNamesPanel_new(Settings* settings) {
   ScreenNamesPanel* this = AllocThis(ScreenNamesPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ScreenNamesFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->renamingItem = NULL;
   memset(this->buffer, 0, sizeof(this->buffer));
   this->ds = NULL;
   this->saved = NULL;
   this->cursor = 0;
   super->cursorOn = false;
   Panel_setHeader(super, "Screens");

   for (unsigned int i = 0; i < settings->nScreens; i++) {
      ScreenSettings* ss = settings->screens[i];
      /* initially show only for Processes tabs (selected) */
      if (ss->dynamic)
         continue;
      Panel_add(super, (Object*) ScreenNameListItem_new(ss->heading, ss));
   }
   return this;
}
