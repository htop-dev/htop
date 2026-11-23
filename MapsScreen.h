#ifndef HEADER_MapsScreen
#define HEADER_MapsScreen
/*
htop - MapsScreen.h
(C) 2025 Pierre Wilke
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct MapsScreen_ {
   InfoScreen super;
} MapsScreen;

extern const InfoScreenClass MapsScreen_class;

MapsScreen* MapsScreen_new(Process* process);

void MapsScreen_delete(Object* this);

#endif
