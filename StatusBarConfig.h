#ifndef HEADER_StatusBarConfig
#define HEADER_StatusBarConfig
/*
htop - StatusBarConfig.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>


typedef struct StatusBarSensorConfig_ {
   char* id;
   bool enabled;
   bool showMin;
   bool showAverage;
   bool showMax;
} StatusBarSensorConfig;

#endif
