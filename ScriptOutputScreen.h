#ifndef HEADER_ScriptOutputScreen
#define HEADER_ScriptOutputScreen
/*
htop - ScriptOutputScreen.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"
#include "RunScript.h"


typedef struct ScriptOutputScreen_ {
   InfoScreen super;
   int read_fd;
   Node* data_head;
   Node** data_tail;
} ScriptOutputScreen;

extern const InfoScreenClass ScriptOutputScreen_class;

ScriptOutputScreen* ScriptOutputScreen_new(const Process* process);

void ScriptOutputScreen_delete(Object* this);

void ScriptOutputScreen_SetFd(ScriptOutputScreen*, int);

#endif
