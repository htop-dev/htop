/*
htop - SolarisProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "solaris/SolarisProcess.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "Process.h"
#include "ProcessList.h"
#include "CRT.h"

#include "solaris/Platform.h"


const ProcessFieldData Process_fields[LAST_PROCESSFIELD] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, O onproc, Z zombie, T stopped, W waiting)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, .pidColumn = true, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, .pidColumn = true, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, .pidColumn = true, },
   [TTY] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   //[TPGID] = { .name = "TPGID", .title = "TPGID", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, .pidColumn = true, },
   //[MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   //[MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [ELAPSED] = { .name = "ELAPSED", .title = "ELAPSED  ", .description = "Time since the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "UID", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = " CPU%", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER       ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, .defaultSortDesc = true, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "TGID", .description = "Thread group ID (i.e. process ID)", .flags = 0, .pidColumn = true, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process", .flags = 0, },
   [CWD] = { .name = "CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_CWD, },
   [ZONEID] = { .name = "ZONEID", .title = "ZONEID", .description = "Zone ID", .flags = 0, .pidColumn = true, },
   [ZONE] = { .name = "ZONE", .title = "ZONE             ", .description = "Zone name", .flags = 0, },
   [PROJID] = { .name = "PROJID", .title = "PRJID", .description = "Project ID", .flags = 0, .pidColumn = true, },
   [TASKID] = { .name = "TASKID", .title = "TSKID", .description = "Task ID", .flags = 0, .pidColumn = true, },
   [POOLID] = { .name = "POOLID", .title = "POLID", .description = "Pool ID", .flags = 0, .pidColumn = true, },
   [CONTID] = { .name = "CONTID", .title = "CNTID", .description = "Contract ID", .flags = 0, .pidColumn = true, },
   [LWPID] = { .name = "LWPID", .title = "LWPID", .description = "LWP ID", .flags = 0, .pidColumn = true, },
};

Process* SolarisProcess_new(const Settings* settings) {
   SolarisProcess* this = xCalloc(1, sizeof(SolarisProcess));
   Object_setClass(this, Class(SolarisProcess));
   Process_init(&this->super, settings);
   return &this->super;
}

void Process_delete(Object* cast) {
   SolarisProcess* sp = (SolarisProcess*) cast;
   Process_done((Process*)cast);
   free(sp->zname);
   free(sp);
}

static void SolarisProcess_writeField(const Process* this, RichString* str, ProcessField field) {
   const SolarisProcess* sp = (const SolarisProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch (field) {
   // add Solaris-specific fields here
   case ZONEID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->zoneid); break;
   case PROJID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->projid); break;
   case TASKID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->taskid); break;
   case POOLID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->poolid); break;
   case CONTID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->contid); break;
   case ZONE: Process_printLeftAlignedField(str, attr, sp->zname ? sp->zname : "global", ZONENAME_MAX/4); return;
   case PID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->realpid); break;
   case PPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->realppid); break;
   case TGID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->realtgid); break;
   case LWPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, sp->lwpid); break;
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_appendWide(str, attr, buffer);
}

static int SolarisProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const SolarisProcess* p1 = (const SolarisProcess*)v1;
   const SolarisProcess* p2 = (const SolarisProcess*)v2;

   switch (key) {
   case ZONEID:
      return SPACESHIP_NUMBER(p1->zoneid, p2->zoneid);
   case PROJID:
      return SPACESHIP_NUMBER(p1->projid, p2->projid);
   case TASKID:
      return SPACESHIP_NUMBER(p1->taskid, p2->taskid);
   case POOLID:
      return SPACESHIP_NUMBER(p1->poolid, p2->poolid);
   case CONTID:
      return SPACESHIP_NUMBER(p1->contid, p2->contid);
   case ZONE:
      return strcmp(p1->zname ? p1->zname : "global", p2->zname ? p2->zname : "global");
   case PID:
      return SPACESHIP_NUMBER(p1->realpid, p2->realpid);
   case PPID:
      return SPACESHIP_NUMBER(p1->realppid, p2->realppid);
   case LWPID:
      return SPACESHIP_NUMBER(p1->lwpid, p2->lwpid);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

const ProcessClass SolarisProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = SolarisProcess_writeField,
   .compareByKey = SolarisProcess_compareByKey
};
