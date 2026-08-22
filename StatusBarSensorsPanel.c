/*
htop - StatusBarSensorsPanel.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "StatusBarSensorsPanel.h"

#include <ctype.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "StatusBar.h"
#include "StatusBarSensorOptionsPanel.h"
#include "XUtils.h"


static const char* const StatusBarSensorsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "      ", "Done  ", NULL};

static void StatusBarSensorListItem_display(const Object* object, RichString* out) {
   const StatusBarSensorListItem* this = (const StatusBarSensorListItem*) object;

   if (this->super.moving) {
      RichString_writeWide(out, CRT_colors[DEFAULT_COLOR],
#ifdef HAVE_LIBNCURSESW
                           CRT_utf8 ? "↕ " :
#endif
                           "+ ");
   }

   char buffer[160];
   xSnprintf(buffer, sizeof(buffer), "[%c] %s", this->enabled ? 'x' : ' ', this->super.value);
   RichString_appendWide(out, CRT_colors[DEFAULT_COLOR], buffer);
}

static void StatusBarSensorListItem_delete(Object* object) {
   StatusBarSensorListItem* this = (StatusBarSensorListItem*) object;
   free(this->sensorId);
   ListItem_delete(object);
}

const ObjectClass StatusBarSensorListItem_class = {
   .extends = Class(ListItem),
   .display = StatusBarSensorListItem_display,
   .delete = StatusBarSensorListItem_delete,
   .compare = ListItem_compare
};

StatusBarSensorListItem* StatusBarSensorListItem_new(const char* value, const char* sensorId) {
   StatusBarSensorListItem* this = AllocThis(StatusBarSensorListItem);
   ListItem_init((ListItem*) this, value, 0);
   this->sensorId = xStrdup(sensorId);
   this->enabled = true;
   this->showMin = false;
   this->showAverage = false;
   this->showMax = false;
   return this;
}

static void StatusBarSensorsPanel_cancelMoving(StatusBarSensorsPanel* this) {
   Panel* super = &this->super;
   for (int i = 0; i < Panel_size(super); i++) {
      ListItem* item = (ListItem*) Panel_get(super, i);
      if (item)
         item->moving = false;
   }
   this->moving = false;
   Panel_setSelectionColor(super, PANEL_SELECTION_FOCUS);
}

void StatusBarSensorsPanel_update(StatusBarSensorsPanel* this) {
   Panel* super = &this->super;
   int size = Panel_size(super);

   for (size_t i = 0; i < this->settings->statusBarSensorCount; i++)
      free(this->settings->statusBarSensors[i].id);
   free(this->settings->statusBarSensors);

   this->settings->statusBarSensors = NULL;
   this->settings->statusBarSensorCount = 0;
   this->settings->statusBarSensorsConfigured = true;

   if (size > 0) {
      this->settings->statusBarSensors = xCalloc((size_t)size, sizeof(StatusBarSensorConfig));
      this->settings->statusBarSensorCount = (size_t)size;

      for (int i = 0; i < size; i++) {
         const StatusBarSensorListItem* item = (const StatusBarSensorListItem*) Panel_get(super, i);
         StatusBarSensorConfig* config = &this->settings->statusBarSensors[i];

         config->id = xStrdup(item->sensorId);
         config->enabled = item->enabled;
         config->showMin = item->showMin;
         config->showAverage = item->showAverage;
         config->showMax = item->showMax;
      }
   }

   this->settings->changed = true;
   this->settings->lastUpdate++;
}

bool StatusBarSensorsPanel_contains(StatusBarSensorsPanel* this, const char* sensorId) {
   Panel* super = &this->super;
   for (int i = 0; i < Panel_size(super); i++) {
      const StatusBarSensorListItem* item = (const StatusBarSensorListItem*) Panel_get(super, i);
      if (item && String_eq(item->sensorId, sensorId))
         return true;
   }
   return false;
}

static void StatusBarSensorsPanel_addConfigured(StatusBarSensorsPanel* this, const char* label, const char* sensorId, bool enabled, bool showMin, bool showAverage, bool showMax) {
   if (StatusBarSensorsPanel_contains(this, sensorId))
      return;

   StatusBarSensorListItem* item = StatusBarSensorListItem_new(label, sensorId);
   item->enabled = enabled;
   item->showMin = showMin;
   item->showAverage = showAverage;
   item->showMax = showMax;

   Panel_add(&this->super, (Object*) item);
}

void StatusBarSensorsPanel_add(StatusBarSensorsPanel* this, const char* label, const char* sensorId) {
   StatusBarSensorsPanel_addConfigured(this, label, sensorId, true, false, false, false);
}

void StatusBarSensorsPanel_fill(StatusBarSensorsPanel* this) {
   Panel* super = &this->super;
   Panel_prune(super);

#if defined(HTOP_LINUX) && defined(HAVE_SENSORS_SENSORS_H)
   size_t count = Platform_getHardwareSensorCount(this->host);
#endif

   if (!this->settings->statusBarSensorsConfigured) {
#if defined(HTOP_LINUX) && defined(HAVE_SENSORS_SENSORS_H)
      for (size_t i = 0; i < count; i++) {
         const char* id = NULL;
         const char* chip = NULL;
         const char* label = NULL;
         const char* feature = NULL;
         HardwareSensorType type;

         if (!Platform_getHardwareSensor(this->host, i, &id, &chip, &label, &feature, &type, NULL) || !id)
            continue;

         char name[64];
         StatusBar_formatSensorName(name, sizeof(name), chip, label, feature, type);
         StatusBarSensorsPanel_add(this, name, id);
      }
#endif
      return;
   }

   for (size_t selection = 0; selection < this->settings->statusBarSensorCount; selection++) {
      const StatusBarSensorConfig* config = &this->settings->statusBarSensors[selection];
      bool found = false;

#if defined(HTOP_LINUX) && defined(HAVE_SENSORS_SENSORS_H)
      for (size_t i = 0; i < count; i++) {
         const char* id = NULL;
         const char* chip = NULL;
         const char* label = NULL;
         const char* feature = NULL;
         HardwareSensorType type;

         if (!Platform_getHardwareSensor(this->host, i, &id, &chip, &label, &feature, &type, NULL) || !id)
            continue;

         if (String_eq(id, config->id)) {
            char name[64];
            StatusBar_formatSensorName(name, sizeof(name), chip, label, feature, type);
            StatusBarSensorsPanel_addConfigured(this, name, id, config->enabled, config->showMin, config->showAverage, config->showMax);
            found = true;
            break;
         }
      }
#endif

      if (!found) {
         char name[256];
         xSnprintf(name, sizeof(name), "%s (unavailable)", config->id);
         StatusBarSensorsPanel_addConfigured(this, name, config->id, config->enabled, config->showMin, config->showAverage, config->showMax);
      }
   }

#if defined(HTOP_LINUX) && defined(HAVE_SENSORS_SENSORS_H)
   for (size_t i = 0; i < count; i++) {
      const char* id = NULL;
      const char* chip = NULL;
      const char* label = NULL;
      const char* feature = NULL;
      HardwareSensorType type;

      if (!Platform_getHardwareSensor(this->host, i, &id, &chip, &label, &feature, &type, NULL) || !id)
         continue;
      if (StatusBarSensorsPanel_contains(this, id))
         continue;

      char name[64];
      StatusBar_formatSensorName(name, sizeof(name), chip, label, feature, type);
      StatusBarSensorsPanel_addConfigured(this, name, id, false, false, false, false);
   }
#endif
}

void StatusBarSensorsPanel_setOptionsPanel(StatusBarSensorsPanel* this, StatusBarSensorOptionsPanel* optionsPanel) {
   this->optionsPanel = optionsPanel;
   if (optionsPanel)
      StatusBarSensorOptionsPanel_setSensor(optionsPanel, (StatusBarSensorListItem*) Panel_getSelected(&this->super));
}

static HandlerResult StatusBarSensorsPanel_eventHandler(Panel* super, int ch) {
   StatusBarSensorsPanel* this = (StatusBarSensorsPanel*) super;
   int oldSelected = super->prevSelected;
   int selected = Panel_getSelectedIndex(super);
   int size = Panel_size(super);
   HandlerResult result = IGNORED;

   switch (ch) {
      case ' ':
         if (selected >= 0 && selected < size) {
            StatusBarSensorListItem* item = (StatusBarSensorListItem*) Panel_getSelected(super);
            if (item) {
               item->enabled = !item->enabled;
               StatusBarSensorsPanel_update(this);
            }
         }
         result = HANDLED;
         break;
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_RECLICK:
         if (selected >= 0 && selected < size) {
            if (this->moving) {
               StatusBarSensorsPanel_cancelMoving(this);
            } else {
               this->moving = true;
               Panel_setSelectionColor(super, PANEL_SELECTION_FOLLOW);
               ListItem* item = (ListItem*) Panel_getSelected(super);
               if (item)
                  item->moving = true;
            }
            result = HANDLED;
         }
         break;
      case KEY_MOUSE:
         if (this->moving) {
            StatusBarSensorsPanel_cancelMoving(this);
            result = HANDLED;
         }
         break;
      case KEY_UP:
         if (!this->moving) {
            Panel_onKey(super, ch);
            result = HANDLED;
            break;
         }
         /* fallthrough */
      case KEY_F(7):
         if (selected > 0 && selected < size) {
            Panel_moveSelectedUp(super);
            StatusBarSensorsPanel_update(this);
         }
         result = HANDLED;
         break;
      case KEY_DOWN:
         if (!this->moving) {
            Panel_onKey(super, ch);
            result = HANDLED;
            break;
         }
         /* fallthrough */
      case KEY_F(8):
         if (selected >= 0 && selected < size - 1) {
            Panel_moveSelectedDown(super);
            StatusBarSensorsPanel_update(this);
         }
         result = HANDLED;
         break;
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END:
         Panel_onKey(super, ch);
         result = HANDLED;
         break;
      case EVENT_SET_SELECTED:
         if (this->moving)
            StatusBarSensorsPanel_cancelMoving(this);
         result = HANDLED;
         break;
      case EVENT_PANEL_LOST_FOCUS:
         if (this->moving)
            StatusBarSensorsPanel_cancelMoving(this);
         result = HANDLED;
         break;
      default:
         if (0 < ch && ch < 255 && isgraph((unsigned char)ch))
            result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP)
            result = IGNORED;
         break;
   }

   StatusBarSensorListItem* newFocus = (StatusBarSensorListItem*) Panel_getSelected(super);
   if (this->optionsPanel && oldSelected != super->selected)
      StatusBarSensorOptionsPanel_setSensor(this->optionsPanel, newFocus);

   super->prevSelected = super->selected;
   return result;
}

static void StatusBarSensorsPanel_delete(Object* object) {
   StatusBarSensorsPanel* this = (StatusBarSensorsPanel*) object;
   Panel_done(&this->super);
   free(this);
}

const PanelClass StatusBarSensorsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = StatusBarSensorsPanel_delete
   },
   .eventHandler = StatusBarSensorsPanel_eventHandler
};

StatusBarSensorsPanel* StatusBarSensorsPanel_new(Machine* host) {
   StatusBarSensorsPanel* this = AllocThis(StatusBarSensorsPanel);
   Panel* super = &this->super;

   FunctionBar* fuBar = FunctionBar_new(StatusBarSensorsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->host = host;
   this->settings = host->settings;
   this->optionsPanel = NULL;
   this->moving = false;

   Panel_setHeader(super, "Sensors");
   StatusBarSensorsPanel_fill(this);

   return this;
}
