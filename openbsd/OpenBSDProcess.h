#ifndef HEADER_OpenBSDProcess
#define HEADER_OpenBSDProcess
/*
htop - OpenBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Machine.h"
#include "Object.h"
#include "Process.h"


typedef struct OpenBSDProcess_ {
   Process super;

   /* 'Kernel virtual addr of u-area' to detect main threads */
   uint64_t addr;
} OpenBSDProcess;

extern const ProcessClass OpenBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* OpenBSDProcess_new(const Machine* host);

void Process_delete(Object* cast);

#endif
