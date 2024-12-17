#ifndef HEADER_FlexMeter
#define HEADER_FlexMeter
/*
htop - FlexMeter.c
(C) 2021 Stoyan Bogdanov
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

#define METERS_LIST_SIZE 30
#define MAX_METERS_COUNT METERS_LIST_SIZE-1

extern MeterClass *FlexMeter_class ;
int load_flex_modules(void);

#endif
