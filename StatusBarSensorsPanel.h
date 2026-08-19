#ifndef HEADER_StatusBarSensorsPanel
#define HEADER_StatusBarSensorsPanel
/*
htop - StatusBarSensorsPanel.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "ListItem.h"
#include "Machine.h"
#include "Panel.h"
#include "Settings.h"


struct StatusBarSensorOptionsPanel_;

typedef struct StatusBarSensorListItem_ {
   ListItem super;
   char* sensorId;
   bool enabled;
   bool showMin;
   bool showAverage;
   bool showMax;
} StatusBarSensorListItem;

typedef struct StatusBarSensorsPanel_ {
   Panel super;
   Machine* host;
   Settings* settings;
   struct StatusBarSensorOptionsPanel_* optionsPanel;
   bool moving;
} StatusBarSensorsPanel;

extern const ObjectClass StatusBarSensorListItem_class;
extern const PanelClass StatusBarSensorsPanel_class;

StatusBarSensorListItem* StatusBarSensorListItem_new(const char* value, const char* sensorId);

StatusBarSensorsPanel* StatusBarSensorsPanel_new(Machine* host);
void StatusBarSensorsPanel_setOptionsPanel(StatusBarSensorsPanel* this, struct StatusBarSensorOptionsPanel_* optionsPanel);
void StatusBarSensorsPanel_fill(StatusBarSensorsPanel* this);

bool StatusBarSensorsPanel_contains(StatusBarSensorsPanel* this, const char* sensorId);
void StatusBarSensorsPanel_add(StatusBarSensorsPanel* this, const char* label, const char* sensorId);
void StatusBarSensorsPanel_update(StatusBarSensorsPanel* this);

#endif
