/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "DisplayOptionsPanel.h"

#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Header.h"
#include "Object.h"
#include "OptionItem.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static const char* const DisplayOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void DisplayOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) super;

   HandlerResult result = IGNORED;
   OptionItem* selected = (OptionItem*) Panel_getSelected(super);

   switch (ch) {
   case '\n':
   case '\r':
   case KEY_ENTER:
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      switch (OptionItem_kind(selected)) {
      case OPTION_ITEM_CHECK:
         CheckItem_toggle((CheckItem*)selected);
         result = HANDLED;
         break;
      case OPTION_ITEM_NUMBER:
         NumberItem_toggle((NumberItem*)selected);
         result = HANDLED;
         break;
      }
      break;
   case '-':
      if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
         NumberItem_decrease((NumberItem*)selected);
         result = HANDLED;
      }
      break;
   case '+':
      if (OptionItem_kind(selected) == OPTION_ITEM_NUMBER) {
         NumberItem_increase((NumberItem*)selected);
         result = HANDLED;
      }
      break;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header* header = this->scr->header;
      Header_calculateHeight(header);
      Header_reinit(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
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
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(DisplayOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(OptionItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Display options");
   Panel_add(super, (Object*) CheckItem_newByRef("Tree view", &(settings->treeView)));
   Panel_add(super, (Object*) CheckItem_newByRef("Shadow other users' processes", &(settings->shadowOtherUsers)));
   Panel_add(super, (Object*) CheckItem_newByRef("Hide kernel threads", &(settings->hideKernelThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Hide userland process threads", &(settings->hideUserlandThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Display threads in a different color", &(settings->highlightThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef("Show custom thread names", &(settings->showThreadNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("Show program path", &(settings->showProgramPath)));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight program \"basename\"", &(settings->highlightBaseName)));
   Panel_add(super, (Object*) CheckItem_newByRef("Merge exe, comm and cmdline in Command", &(settings->showMergedCommand)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Try to find comm in cmdline (when Command is merged)", &(settings->findCommInCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Try to strip exe from cmdline (when Command is merged)", &(settings->stripExeFromCmdline)));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight large numbers in memory counters", &(settings->highlightMegabytes)));
   Panel_add(super, (Object*) CheckItem_newByRef("Leave a margin around header", &(settings->headerMargin)));
   Panel_add(super, (Object*) CheckItem_newByRef("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)", &(settings->detailedCPUTime)));
   Panel_add(super, (Object*) CheckItem_newByRef("Count CPUs from 1 instead of 0", &(settings->countCPUsFromOne)));
   Panel_add(super, (Object*) CheckItem_newByRef("Update process names on every refresh", &(settings->updateProcessNames)));
   Panel_add(super, (Object*) CheckItem_newByRef("Add guest time in CPU meter percentage", &(settings->accountGuestInCPUMeter)));
   Panel_add(super, (Object*) CheckItem_newByRef("Also show CPU percentage numerically", &(settings->showCPUUsage)));
   Panel_add(super, (Object*) CheckItem_newByRef("Also show CPU frequency", &(settings->showCPUFrequency)));
   #ifdef HAVE_SENSORS_SENSORS_H
   Panel_add(super, (Object*) CheckItem_newByRef("Also show CPU temperature (requires libsensors)", &(settings->showCPUTemperature)));
   Panel_add(super, (Object*) CheckItem_newByRef("- Show temperature in degree Fahrenheit instead of Celsius", &(settings->degreeFahrenheit)));
   #endif
   Panel_add(super, (Object*) CheckItem_newByRef("Enable the mouse", &(settings->enableMouse)));
   Panel_add(super, (Object*) NumberItem_newByRef("Update interval (in seconds)", &(settings->delay), -1, 1, 255));
   Panel_add(super, (Object*) CheckItem_newByRef("Highlight new and old processes", &(settings->highlightChanges)));
   Panel_add(super, (Object*) NumberItem_newByRef("- Highlight time (in seconds)", &(settings->highlightDelaySecs), 0, 1, 24*60*60));
   #ifdef HAVE_LIBHWLOC
   Panel_add(super, (Object*) CheckItem_newByRef("Show topology when selecting affinity by default", &(settings->topologyAffinity)));
   #endif
   return this;
}
