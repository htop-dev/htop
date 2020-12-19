#ifndef HEADER_UnsupportedProcess
#define HEADER_UnsupportedProcess
/*
htop - UnsupportedProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"

#define Process_delete UnsupportedProcess_delete

extern ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* UnsupportedProcess_new(Settings* settings);

void UnsupportedProcess_delete(Object* cast);

#endif
