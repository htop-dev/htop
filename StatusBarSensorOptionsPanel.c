/*
htop - StatusBarSensorOptionsPanel.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "StatusBarSensorOptionsPanel.h"

#include <stdlib.h>

#include "FunctionBar.h"
#include "OptionItem.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static const char* const StatusBarSensorOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "Off   ", "On    ", "      ", "Done  ", NULL};

static void StatusBarSensorOptionsPanel_fill(StatusBarSensorOptionsPanel* this) {
   Panel* super = &this->super;
   int selected = Panel_getSelectedIndex(super);

   Panel_prune(super);
   Panel_setHeader(super, "Sensor options");

   if (!this->sensor)
      return;

   Panel_add(super, (Object*) CheckItem_newByRef("Min", &this->sensor->showMin));
   Panel_add(super, (Object*) CheckItem_newByRef("Avg", &this->sensor->showAverage));
   Panel_add(super, (Object*) CheckItem_newByRef("Max", &this->sensor->showMax));

   if (selected < 0)
      selected = 0;
   if (selected >= Panel_size(super))
      selected = Panel_size(super) - 1;
   if (selected >= 0)
      Panel_setSelected(super, selected);
}

static HandlerResult StatusBarSensorOptionsPanel_eventHandler(Panel* super, int ch) {
   StatusBarSensorOptionsPanel* this = (StatusBarSensorOptionsPanel*) super;
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   if (!this->sensor || !selected)
      return IGNORED;

   bool changed = false;
   HandlerResult result = IGNORED;

   switch (ch) {
   case ' ':
   case '\n':
   case '\r':
   case KEY_ENTER:
   case KEY_RECLICK:
      CheckItem_toggle(selected);
      changed = true;
      result = HANDLED;
      break;
   case '-':
   case KEY_PADMINUS:
   case KEY_F(7):
      CheckItem_set(selected, false);
      changed = true;
      result = HANDLED;
      break;
   case '+':
   case KEY_PADPLUS:
   case KEY_F(8):
      CheckItem_set(selected, true);
      changed = true;
      result = HANDLED;
      break;
   case KEY_UP:
   case KEY_DOWN:
   case KEY_NPAGE:
   case KEY_PPAGE:
   case KEY_HOME:
   case KEY_END:
      Panel_onKey(super, ch);
      result = HANDLED;
      break;
   default:
      break;
   }

   if (changed)
      StatusBarSensorsPanel_update(this->sensorsPanel);

   return result;
}

static void StatusBarSensorOptionsPanel_delete(Object* object) {
   StatusBarSensorOptionsPanel* this = (StatusBarSensorOptionsPanel*) object;
   Panel_done(&this->super);
   free(this);
}

const PanelClass StatusBarSensorOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = StatusBarSensorOptionsPanel_delete
   },
   .eventHandler = StatusBarSensorOptionsPanel_eventHandler
};

void StatusBarSensorOptionsPanel_setSensor(StatusBarSensorOptionsPanel* this, StatusBarSensorListItem* sensor) {
   this->sensor = sensor;
   StatusBarSensorOptionsPanel_fill(this);
}

StatusBarSensorOptionsPanel* StatusBarSensorOptionsPanel_new(StatusBarSensorsPanel* sensorsPanel) {
   StatusBarSensorOptionsPanel* this = AllocThis(StatusBarSensorOptionsPanel);
   Panel* super = &this->super;

   FunctionBar* fuBar = FunctionBar_new(StatusBarSensorOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(OptionItem), true, fuBar);

   this->sensorsPanel = sensorsPanel;
   this->sensor = NULL;

   Panel_setHeader(super, "Sensor options");

   return this;
}
