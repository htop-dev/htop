#ifndef HEADER_BacktraceScreen
#define HEADER_BacktraceScreen
/*
htop - BacktraceScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#ifdef BACKTRACE_ENABLED
#include <stddef.h>

#include "Panel.h"
#include "Process.h"

typedef struct BacktracePanel_ {
   Panel super;
   const Process* process;
} BacktracePanel;

typedef struct Frame_ {
   Object super;
   int index;
   size_t address;
   size_t offset;
   char* functionName;
   bool isSignalFrame;
} Frame;

BacktracePanel* BacktracePanel_new(const Process* process);
void BacktracePanel_delete(Object* object);
Frame* Frame_new(void);

extern const PanelClass BacktracePanel_class;
extern const ObjectClass Frame_class;

#endif
#endif
