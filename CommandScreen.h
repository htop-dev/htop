#ifndef HEADER_CommandScreen
#define HEADER_CommandScreen
/*
htop - CommandScreen.h
(C) 2017,2020 ryenus
(C) 2020,2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct CommandScreen_ {
   InfoScreen super;
} CommandScreen;

extern const InfoScreenClass CommandScreen_class;

CommandScreen* CommandScreen_new(Process* process);

void CommandScreen_delete(Object* this);

#endif
