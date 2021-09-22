#ifndef HEADER_UnsupportedProcess
#define HEADER_UnsupportedProcess
/*
htop - UnsupportedProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"


typedef struct UnsupportedProcess_ {
   Process super;

   /* Add platform specific fields */
} UnsupportedProcess;


extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* UnsupportedProcess_new(const Settings* settings);

void Process_delete(Object* cast);

extern const ProcessClass UnsupportedProcess_class;

#endif
