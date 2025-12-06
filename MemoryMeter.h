#ifndef HEADER_MemoryMeter
#define HEADER_MemoryMeter
/*
htop - MemoryMeter.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef struct MemoryClass_s {
   const char *label; // e.g. "wired", "shared", "compressed" - platform-specific memory classe names
   bool countsAsUsed; // memory class counts as "used" memory
   bool countsAsCache; // memory class reclaimed under pressure (displayed with "show cached memory")
   ColorElements color; // one of the MEMORY CRT color values
} MemoryClass;

extern const MeterClass MemoryMeter_class;

#endif
