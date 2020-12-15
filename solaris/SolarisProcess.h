#ifndef HEADER_SolarisProcess
#define HEADER_SolarisProcess
/*
htop - SolarisProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include <zone.h>
#include <sys/proc.h>
#include <libproc.h>

typedef struct SolarisProcess_ {
   Process    super;
   int        kernel;
   zoneid_t   zoneid;
   char*      zname;
   taskid_t   taskid;
   projid_t   projid;
   poolid_t   poolid;
   ctid_t     contid;
   bool       is_lwp;
   pid_t      realpid;
   pid_t      realppid;
   pid_t      lwpid;
} SolarisProcess;

#define Process_isKernelThread(_process) (_process->kernel == 1)

#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)

extern const ProcessClass SolarisProcess_class;

extern ProcessFieldData Process_fields[LAST_PROCESSFIELD];

extern ProcessPidColumn Process_pidColumns[];

Process* SolarisProcess_new(const Settings* settings);

void Process_delete(Object* cast);

void SolarisProcess_writeField(const Process* this, RichString* str, ProcessField field);

long SolarisProcess_compareByKey(const Process* v1, const Process* v2, ProcessField field);

bool Process_isThread(const Process* this);

#endif
