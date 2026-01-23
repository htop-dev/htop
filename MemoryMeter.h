#ifndef HEADER_MemoryMeter
#define HEADER_MemoryMeter
/*
htop - MemoryMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

typedef struct MemoryClass_s {
   const char *label; // e.g. "used", "shared", "compressed", etc. Should reflect the system-specific 'top' classes
   bool countsAsUsed; // whether this memory class counts as "used" memory
   bool countsAsCache; // whether this memory class can be reclaimed under pressure (and displayed when "show cached memory" is checked)
   ColorElements color; // one of the DYNAMIC_xxx CRT color values
} MemoryClass;

extern const MemoryClass Platform_memoryClasses[]; // defined in the platform-specific code
extern const unsigned int Platform_numberOfMemoryClasses; // defined in the platform-specific code

extern const MeterClass MemoryMeter_class;

#endif
