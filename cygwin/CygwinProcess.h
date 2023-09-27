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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <windows.h>  // for DWORD
#undef WIN32_LEAN_AND_MEAN

#include "Machine.h"
#include "Object.h"
#include "Process.h"


typedef struct CygwinProcess_ {
   Process super;
   unsigned long int cminflt;
   unsigned long int cmajflt;
   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long m_share;
   long m_trs;
   long m_drs;
   long m_lrs;

   /* Process flags */
   unsigned long int flags;

   DWORD winpid;
} CygwinProcess;

extern const ProcessClass CygwinProcess_class;

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

Process* CygwinProcess_new(const Machine* host);

void Process_delete(Object* super);

#endif /* HEADER_CygwinProcess */
