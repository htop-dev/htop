#ifndef RUNSCRIPT_Process
#define RUNSCRIPT_Process
/*
htop - RunScript.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Action.h"


typedef struct Node_ {
   char* line;
   struct Node_* next;
} Node;


void RunScript(State*);

void root_exec(const char*, bool);

#endif
