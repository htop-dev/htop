#ifndef HEADER_NetBSDProcess
#define HEADER_NetBSDProcess
/*
htop - NetBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Machine.h"
#include "Object.h"
#include "Process.h"


typedef struct NetBSDProcess_ {
   Process super;
} NetBSDProcess;

extern const ProcessClass NetBSDProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* NetBSDProcess_new(const Machine* host);

void Process_delete(Object* cast);

#endif
