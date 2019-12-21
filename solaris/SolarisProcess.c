/*
htop - SolarisProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "SolarisProcess.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>


ProcessClass SolarisProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = SolarisProcess_compare
   },
   .writeField = (Process_WriteField) SolarisProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID    ", .description = "Process/thread ID", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, O onproc, Z zombie, T stopped, W waiting)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = "  UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [ZONEID] = { .name = "ZONEID", .title = " ZONEID ", .description = "Zone ID", .flags = 0, },
   [ZONE] = { .name = "ZONE", .title = "ZONE             ", .description = "Zone name", .flags = 0, },
   [PROJID] = { .name = "PROJID", .title = " PRJID ", .description = "Project ID", .flags = 0, },
   [TASKID] = { .name = "TASKID", .title = " TSKID ", .description = "Task ID", .flags = 0, },
   [POOLID] = { .name = "POOLID", .title = " POLID ", .description = "Pool ID", .flags = 0, },
   [CONTID] = { .name = "CONTID", .title = " CNTID ", .description = "Contract ID", .flags = 0, },
   [LWPID] = { .name = "LWPID", .title = " LWPID ", .description = "LWP ID", .flags = 0, },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = ZONEID, .label = "ZONEID" },
   { .id = TASKID, .label = "TSKID" },
   { .id = PROJID, .label = "PRJID" },
   { .id = POOLID, .label = "POLID" },
   { .id = CONTID, .label = "CNTID" },
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   { .id = LWPID, .label = "LWPID" },
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = 0, .label = NULL },
};

SolarisProcess* SolarisProcess_new(Settings* settings) {
   SolarisProcess* this = xCalloc(1, sizeof(SolarisProcess));
   Object_setClass(this, Class(SolarisProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   SolarisProcess* sp = (SolarisProcess*) cast;
   Process_done((Process*)cast);
   free(sp->zname);
   free(sp);
}

void SolarisProcess_writeField(Process* this, RichString* str, ProcessField field) {
   SolarisProcess* sp = (SolarisProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int) field) {
   // add Solaris-specific fields here
   case ZONEID: xSnprintf(buffer, n, Process_pidFormat, sp->zoneid); break;
   case PROJID: xSnprintf(buffer, n, Process_pidFormat, sp->projid); break;
   case TASKID: xSnprintf(buffer, n, Process_pidFormat, sp->taskid); break;
   case POOLID: xSnprintf(buffer, n, Process_pidFormat, sp->poolid); break;
   case CONTID: xSnprintf(buffer, n, Process_pidFormat, sp->contid); break;
   case ZONE:{
      xSnprintf(buffer, n, "%-*s ", ZONENAME_MAX/4, sp->zname); break;
      if (buffer[ZONENAME_MAX/4] != '\0') {
         buffer[ZONENAME_MAX/4] = ' ';
         buffer[(ZONENAME_MAX/4)+1] = '\0';
      }
      break;
   }
   case PID: xSnprintf(buffer, n, Process_pidFormat, sp->realpid); break;
   case PPID: xSnprintf(buffer, n, Process_pidFormat, sp->realppid); break;
   case LWPID: xSnprintf(buffer, n, Process_pidFormat, sp->lwpid); break;
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long SolarisProcess_compare(const void* v1, const void* v2) {
   SolarisProcess *p1, *p2;
   Settings* settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (SolarisProcess*)v1;
      p2 = (SolarisProcess*)v2;
   } else {
      p2 = (SolarisProcess*)v1;
      p1 = (SolarisProcess*)v2;
   }
   switch ((int) settings->sortKey) {
   case ZONEID:
      return (p1->zoneid - p2->zoneid);
   case PROJID:
      return (p1->projid - p2->projid);
   case TASKID:
      return (p1->taskid - p2->taskid);
   case POOLID:
      return (p1->poolid - p2->poolid);
   case CONTID:
      return (p1->contid - p2->contid);
   case ZONE:
      return strcmp(p1->zname ? p1->zname : "global", p2->zname ? p2->zname : "global");
   case PID:
      return (p1->realpid - p2->realpid);
   case PPID:
      return (p1->realppid - p2->realppid);
   case LWPID:
      return (p1->lwpid - p2->lwpid);
   default:
      return Process_compare(v1, v2);
   }
}

bool Process_isThread(Process* this) {
   SolarisProcess* fp = (SolarisProcess*) this;

   if (fp->kernel == 1 ) {
      return 1;
   } else if (fp->is_lwp) {
      return 1;
   } else {
      return 0;
   }
}
