#ifndef HEADER_CygwinProcessTable
#define HEADER_CygwinProcessTable
/*
htop - CygwinProcessTable.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessTable.h"


typedef struct CygwinProcessTable_ {
   ProcessTable super;
} CygwinProcessTable;

#endif /* HEADER_CygwinProcessTable */
