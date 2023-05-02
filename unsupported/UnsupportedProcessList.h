#ifndef HEADER_UnsupportedProcessList
#define HEADER_UnsupportedProcessList
/*
htop - UnsupportedProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"


typedef struct UnsupportedProcessList_ {
   ProcessList super;
} UnsupportedProcessList;

#endif
