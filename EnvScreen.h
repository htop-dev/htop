#ifndef HEADER_EnvScreen
#define HEADER_EnvScreen
/*
htop - EnvScreen.h
(C) 2015,2016 Michael Klein
(C) 2016,2017 Hisham H. Muhammad
(C) 2020,2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct EnvScreen_ {
   InfoScreen super;
} EnvScreen;

extern const InfoScreenClass EnvScreen_class;

EnvScreen* EnvScreen_new(Process* process);

void EnvScreen_delete(Object* this);

#endif
