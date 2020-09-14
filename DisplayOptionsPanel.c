/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DisplayOptionsPanel.h"

#include "CheckItem.h"
#include "CRT.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


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
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      CheckItem_set(selected, ! (CheckItem_get(selected)) );
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      const Header* header = this->scr->header;
      Header_calculateHeight((Header*) header);
      Header_reinit((Header*) header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

PanelClass DisplayOptionsPanel_class = {
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
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Display options");
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Tree view"), &(settings->treeView)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Shadow other users' processes"), &(settings->shadowOtherUsers)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Hide kernel threads"), &(settings->hideKernelThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Hide userland process threads"), &(settings->hideUserlandThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Display threads in a different color"), &(settings->highlightThreads)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Show custom thread names"), &(settings->showThreadNames)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Show program path"), &(settings->showProgramPath)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Highlight program \"basename\""), &(settings->highlightBaseName)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Highlight large numbers in memory counters"), &(settings->highlightMegabytes)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Leave a margin around header"), &(settings->headerMargin)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)"), &(settings->detailedCPUTime)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Count CPUs from 0 instead of 1"), &(settings->countCPUsFromZero)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Update process names on every refresh"), &(settings->updateProcessNames)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Add guest time in CPU meter percentage"), &(settings->accountGuestInCPUMeter)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Also show CPU percentage numerically"), &(settings->showCPUUsage)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Also show CPU frequency"), &(settings->showCPUFrequency)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Enable the mouse"), &(settings->enableMouse)));
   #ifdef HAVE_LIBHWLOC
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Show topology when selecting affinity by default"), &(settings->topologyAffinity)));
   #endif
   return this;
}
