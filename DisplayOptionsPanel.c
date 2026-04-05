/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DisplayOptionsPanel.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Header.h"
#include "Object.h"
#include "OptionItem.h"
#include "ProvideCurses.h"
#include "ScreensPanel.h"


static const char* const DisplayOptionsFunctions[] =       {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static const char* const DisplayOptionsDecIncFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "Dec   ", "Inc   ", "      ", "Done  ", NULL};

static void DisplayOptionsPanel_delete(Object* object) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) object;
   FunctionBar_delete(this->decIncBar);
   Panel_done(&this->super);
   free(this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) super;

   HandlerResult result = IGNORED;
   bool settingsChanged = false;
   OptionItem* selected = (OptionItem*) Panel_getSelected(super);

   if (!selected) {
      return result;
   }

   NumberItem* numItem = (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) ? (NumberItem*)selected : NULL;

   /* Helper: position the hardware cursor right after the edit buffer.
    * +1 on Y for the panel header row; +1 on X for the leading '[' bracket. */
   #define SET_EDIT_CURSOR() do { \
      super->cursorY = super->y + 1 + (super->selected - super->scrollV); \
      super->cursorX = super->x + 1 + numItem->editLen; \
      super->cursorOn = true; \
   } while (0)

   switch (ch) {
      case 27: /* Escape: cancel editing */
         if (numItem && numItem->editing) {
            NumberItem_cancelEditing(numItem);
            super->cursorOn = false;
            return HANDLED;
         }
         break;
      case KEY_BACKSPACE:
      case KEY_DEL_MAC:
         if (numItem) {
            if (!numItem->editing) {
               NumberItem_startEditingFromValue(numItem);
            }
            NumberItem_deleteChar(numItem);
            SET_EDIT_CURSOR();
            return HANDLED;
         }
         break;
      case '\n':
      case '\r':
      case KEY_ENTER:
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
            result = HANDLED;
            break;
         }
         /* When not editing, fall through to space/toggle */
         /* fallthrough */
      case ' ':
         if (numItem && numItem->editing) {
            /* Space while editing: apply pending edit, then toggle */
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         }
         if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
            NumberItem_toggle((NumberItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         } else if (OptionItem_kind(selected) == OPTION_ITEM_CHECK) {
            CheckItem_toggle((CheckItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         }
         break;
      case '-':
      case KEY_PADMINUS:
      case KEY_F(7):
      case KEY_RIGHTCLICK:
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         }
         if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
            NumberItem_decrease((NumberItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         } else if (OptionItem_kind(selected) == OPTION_ITEM_CHECK) {
            CheckItem_set((CheckItem*)selected, false);
            result = HANDLED;
            settingsChanged = true;
         }
         break;
      case '+':
      case KEY_PADPLUS:
      case KEY_F(8):
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         }
         if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
            NumberItem_increase((NumberItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         } else if (OptionItem_kind(selected) == OPTION_ITEM_CHECK) {
            CheckItem_set((CheckItem*)selected, true);
            result = HANDLED;
            settingsChanged = true;
         }
         break;
      case KEY_RECLICK:
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         }
         if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
            NumberItem_increase((NumberItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         } else if (OptionItem_kind(selected) == OPTION_ITEM_CHECK) {
            CheckItem_toggle((CheckItem*)selected);
            result = HANDLED;
            settingsChanged = true;
         }
         break;
      case KEY_UP:
      case KEY_DOWN:
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END:
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         }
         {
            OptionItem* previous = selected;
            Panel_onKey(super, ch);
            selected = (OptionItem*) Panel_getSelected(super);
            if (previous != selected) {
               result = HANDLED;
               settingsChanged = true;
            }
         }
         /* FALLTHROUGH */
      case EVENT_SET_SELECTED:
         super->cursorOn = false;
         if (selected && OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
            super->currentBar = this->decIncBar;
         } else {
            Panel_setDefaultBar(super);
         }
         break;
      case EVENT_PANEL_LOST_FOCUS:
         if (numItem && numItem->editing) {
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
         }
         super->cursorOn = false;
         Panel_setDefaultBar(super);
         break;
      default:
         if (numItem && numItem->editing) {
            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == ',') {
               NumberItem_addChar(numItem, (char)ch);
               SET_EDIT_CURSOR();
               return HANDLED;
            }
            /* For navigation, inc/dec, and other keys: apply pending edit first */
            if (NumberItem_applyEditing(numItem)) {
               settingsChanged = true;
            }
            super->cursorOn = false;
         } else if (numItem) {
            /* Start editing when a digit or decimal separator is typed */
            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == ',') {
               NumberItem_startEditing(numItem);
               NumberItem_addChar(numItem, (char)ch);
               SET_EDIT_CURSOR();
               return HANDLED;
            }
         }
         break;
   }

   #undef SET_EDIT_CURSOR

   if (settingsChanged) {
      this->settings->changed = true;
      this->settings->lastUpdate++;
      CRT_updateDelay();
      Header* header = this->scr->header;
      Header_calculateHeight(header);
      Header_reinit(header);
      Header_updateData(header);
      Header_draw(header);
      ScreenManager_resize(this->scr);
   }

   return result;
}

const PanelClass DisplayOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DisplayOptionsPanel_delete
   },
   .eventHandler = DisplayOptionsPanel_eventHandler
};

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsPanel* this = AllocThis(DisplayOptionsPanel);
   Panel* super = &this->super;

   FunctionBar* fuBar = FunctionBar_new(DisplayOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(OptionItem), true, fuBar);

   this->decIncBar = FunctionBar_new(DisplayOptionsDecIncFunctions, NULL, NULL);
   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Display options");

   #define TABMSG "For current screen tab: \0"
   char tabheader[sizeof(TABMSG) + SCREEN_NAME_LEN + 1] = TABMSG;
   strncat(tabheader, settings->ss->heading, SCREEN_NAME_LEN);
   Panel_add(super, (Object*) TextItem_new(tabheader));
   #undef TABMSG

   Panel_add(super, (Object*) CheckItem_newByRef("Tree view", &(settings->ss->treeView)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Tree view is always sorted by PID (htop 2 behavior)", &(settings->ss->treeViewAlwaysByPID)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Tree view is collapsed by default", &(settings->ss->allBranchesCollapsed)));
   Panel_add(super, (Object*) TextItem_new("Global options:"));
   Panel_add(super, (Object*) CheckItem_newByRef("Show tabs for screens", &(settings->screenTabs)));
   Panel_add(super, (Object*) CheckItem_newByRef("Shadow other users' processes", &(settings->shadowOtherUsers)));
   Panel_add(super, (Object*) CheckItem_newByRef("Hide kernel threads", &(settings->hideKernelThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Hide userland process threads", &(settings->hideUserlandThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Hide processes running in containers", &(settings->hideRunningInContainer)));
   Panel_add(super, (Object*) CheckItem_newByRef("Display threads in a different color", &(settings->highlightThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Show custom thread names", &(settings->showThreadNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("Show program path", &(settings->showProgramPath)));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight program \"basename\"", &(settings->highlightBaseName)));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight out-dated/removed programs (red) / libraries (yellow)", &(settings->highlightDeletedExe)));
   Panel_add(super, (Object*) CheckItem_newByRef("Shadow distribution path prefixes", &(settings->shadowDistPathPrefix)));
   Panel_add(super, (Object*) CheckItem_newByRef("Merge exe, comm and cmdline in Command", &(settings->showMergedCommand)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Try to find comm in cmdline (when Command is merged)", &(settings->findCommInCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Try to strip exe from cmdline (when Command is merged)", &(settings->stripExeFromCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight large numbers in memory counters", &(settings->highlightMegabytes)));
   Panel_add(super, (Object*) CheckItem_newByRef("Leave a margin around header", &(settings->headerMargin)));
   Panel_add(super, (Object*) CheckItem_newByRef("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)", &(settings->detailedCPUTime)));
   Panel_add(super, (Object*) CheckItem_newByRef("Count CPUs from 1 instead of 0", &(settings->countCPUsFromOne)));
   Panel_add(super, (Object*) CheckItem_newByRef("Label CPUs based on SMT topology (e.g. 0a, 0b) instead of CPU index", &(settings->showCPUSMTLabels)));
   Panel_add(super, (Object*) CheckItem_newByRef("Update process names on every refresh", &(settings->updateProcessNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("Add guest time in CPU meter percentage", &(settings->accountGuestInCPUMeter)));
   Panel_add(super, (Object*) CheckItem_newByRef("Also show CPU percentage numerically", &(settings->showCPUUsage)));
   Panel_add(super, (Object*) CheckItem_newByRef("Also show CPU frequency", &(settings->showCPUFrequency)));
   #ifdef BUILD_WITH_CPU_TEMP
   Panel_add(super, (Object*) CheckItem_newByRef(
   #if defined(HTOP_LINUX)
                                                 "Also show CPU temperature (requires libsensors)",
   #elif defined(HTOP_FREEBSD)
                                                 "Also show CPU temperature",
   #else
   #error Unknown temperature implementation!
   #endif
                                                 &(settings->showCPUTemperature)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Show temperature in degree Fahrenheit instead of Celsius", &(settings->degreeFahrenheit)));
   #endif
   Panel_add(super, (Object*) CheckItem_newByRef("Show cached memory in graph and bar modes", &(settings->showCachedMemory)));
   #ifdef HAVE_GETMOUSE
   Panel_add(super, (Object*) CheckItem_newByRef("Enable the mouse", &(settings->enableMouse)));
   #endif
   Panel_add(super, (Object*) NumberItem_newByRef("Update interval (in seconds)", &(settings->delay), -1, 1, 255));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight new and old processes", &(settings->highlightChanges)));
   Panel_add(super, (Object*) NumberItem_newByRef("- Highlight time (in seconds)", &(settings->highlightDelaySecs), 0, 1, 24 * 60 * 60));
   Panel_add(super, (Object*) NumberItem_newByRef("Hide main function bar (0 - off, 1 - on ESC until next input, 2 - permanently)", &(settings->hideFunctionBar), 0, 0, 2));
   #ifdef HAVE_LIBHWLOC
   Panel_add(super, (Object*) CheckItem_newByRef("Show topology when selecting affinity by default", &(settings->topologyAffinity)));
   #endif

   return this;
}
