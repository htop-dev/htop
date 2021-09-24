#ifndef HEADER_OpenFilesScreen
#define HEADER_OpenFilesScreen
/*
htop - OpenFilesScreen.h
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/types.h>

#include "InfoScreen.h"
#include "Object.h"
#include "Process.h"


typedef struct OpenFilesScreen_ {
   InfoScreen super;
   pid_t pid;
} OpenFilesScreen;

extern const InfoScreenClass OpenFilesScreen_class;

OpenFilesScreen* OpenFilesScreen_new(const Process* process);

void OpenFilesScreen_delete(Object* this);

#endif
