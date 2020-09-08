#ifndef HEADER_UnsupportedProcess
#define HEADER_UnsupportedProcess
/*
htop - UnsupportedProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"

#define Process_delete UnsupportedProcess_delete

Process* UnsupportedProcess_new(Settings* settings);

void UnsupportedProcess_delete(Object* cast);

#endif
