#ifndef HEADER_MeterMode
#define HEADER_MeterMode
/*
htop - MeterMode.h
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/


enum MeterModeId_ {
   /* Meter mode 0 is reserved */
   BAR_METERMODE = 1,
   TEXT_METERMODE,
   GRAPH_METERMODE,
   LED_METERMODE,
   LAST_METERMODE
};

typedef unsigned int MeterModeId;

#define METERMODE_DEFAULT_SUPPORTED ( \
   (1 << BAR_METERMODE) |             \
   (1 << TEXT_METERMODE) |            \
   (1 << GRAPH_METERMODE) |           \
   (1 << LED_METERMODE) |             \
   0) // Avoids edits when updating

#endif
