#ifndef HEADER_StatusBarSensorOptionsPanel
#define HEADER_StatusBarSensorOptionsPanel
/*
htop - StatusBarSensorOptionsPanel.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"
#include "Panel.h"
#include "StatusBarSensorsPanel.h"


typedef struct StatusBarSensorOptionsPanel_ {
   Panel super;
   StatusBarSensorsPanel* sensorsPanel;
   StatusBarSensorListItem* sensor;
} StatusBarSensorOptionsPanel;

extern const PanelClass StatusBarSensorOptionsPanel_class;

StatusBarSensorOptionsPanel* StatusBarSensorOptionsPanel_new(StatusBarSensorsPanel* sensorsPanel);
void StatusBarSensorOptionsPanel_setSensor(StatusBarSensorOptionsPanel* this, StatusBarSensorListItem* sensor);

#endif
