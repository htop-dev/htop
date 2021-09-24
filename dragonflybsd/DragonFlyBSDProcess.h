#ifndef HEADER_DragonFlyBSDProcess
#define HEADER_DragonFlyBSDProcess
/*
htop - dragonflybsd/DragonFlyBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Object.h"
#include "Process.h"
#include "Settings.h"


typedef struct DragonFlyBSDProcess_ {
   Process super;
   int   jid;
   char* jname;
} DragonFlyBSDProcess;

extern const ProcessClass DragonFlyBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* DragonFlyBSDProcess_new(const Settings* settings);

void Process_delete(Object* cast);

#endif
