#ifndef HEADER_CygwinProcess
#define HEADER_CygwinProcess
/*
htop - CygwinProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Machine.h"
#include "Object.h"
#include "Process.h"


typedef struct CygwinProcess_ {
   Process super;
} CygwinProcess;

extern const ProcessClass CygwinProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* CygwinProcess_new(const Machine* host);

void Process_delete(Object* super);

#endif /* HEADER_CygwinProcess */
