#ifndef HEADER_ProcessLocksScreen
#define HEADER_ProcessLocksScreen
/*
htop - ProcessLocksScreen.h
(C) 2020 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "InfoScreen.h"

#include <stdlib.h>


typedef struct ProcessLocksScreen_ {
   InfoScreen super;
   pid_t pid;
} ProcessLocksScreen;

extern const InfoScreenClass ProcessLocksScreen_class;

ProcessLocksScreen* ProcessLocksScreen_new(const Process* process);

void ProcessLocksScreen_delete(Object* this);

#endif
