/*
htop - FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "freebsd/FreeBSDProcess.h"

#include <stdlib.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "RichString.h"
#include "Scheduling.h"
#include "XUtils.h"


const char* const nodevStr = "nodev";

const ProcessFieldData Process_fields[LAST_PROCESSFIELD] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, .pidColumn = true, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, .pidColumn = true, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, .pidColumn = true, },
   [TTY] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "TPGID", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, .pidColumn = true, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of copy-on-write faults", .flags = 0, .defaultSortDesc = true, },
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
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, .defaultSortDesc = true, },
   [TGID] = { .name = "TGID", .title = "TGID", .description = "Thread group ID (i.e. process ID)", .flags = 0, .pidColumn = true, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process", .flags = 0, },
   [CWD] = { .name = "CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_CWD, },
#ifdef SCHEDULER_SUPPORT
   [SCHEDULERPOLICY] = { .name = "SCHEDULERPOLICY", .title = "SCHED ", .description = "Current scheduling policy of the process", .flags = PROCESS_FLAG_SCHEDPOL, },
#endif
   [JID] = { .name = "JID", .title = "JID", .description = "Jail prison ID", .flags = 0, .pidColumn = true, },
   [JAIL] = { .name = "JAIL", .title = "JAIL        ", .description = "Jail prison name", .flags = 0, },
   [EMULATION] = { .name = "EMULATION", .title = "EMULATION        ", .description = "System call emulation environment (ABI)", .flags = 0, },
};

Process* FreeBSDProcess_new(const Machine* machine) {
   FreeBSDProcess* this = xCalloc(1, sizeof(FreeBSDProcess));
   Object_setClass(this, Class(FreeBSDProcess));
   Process_init(&this->super, machine);
   return &this->super;
}

void Process_delete(Object* cast) {
   FreeBSDProcess* this = (FreeBSDProcess*) cast;
   Process_done((Process*)cast);
   free(this->emul);
   free(this->jname);
   free(this);
}

static void FreeBSDProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const FreeBSDProcess* fp = (const FreeBSDProcess*) super;

   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;

   switch (field) {
   // add FreeBSD-specific fields here
   case JID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, fp->jid); break;
   case JAIL:
      Row_printLeftAlignedField(str, attr, fp->jname ? fp->jname : "N/A", 11);
      return;
   case EMULATION:
      Row_printLeftAlignedField(str, attr, fp->emul ? fp->emul : "N/A", 16);
      return;
   default:
      Process_writeField(&fp->super, str, field);
      return;
   }

   RichString_appendWide(str, attr, buffer);
}

static int FreeBSDProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const FreeBSDProcess* p1 = (const FreeBSDProcess*)v1;
   const FreeBSDProcess* p2 = (const FreeBSDProcess*)v2;

   switch (key) {
   // add FreeBSD-specific fields here
   case JID:
      return SPACESHIP_NUMBER(p1->jid, p2->jid);
   case JAIL:
      return SPACESHIP_NULLSTR(p1->jname, p2->jname);
   case EMULATION:
      return SPACESHIP_NULLSTR(p1->emul, p2->emul);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

const ProcessClass FreeBSDProcess_class = {
   .super = {
      .super = {
         .extends = Class(Process),
         .display = Row_display,
         .delete = Process_delete,
         .compare = Process_compare
      },
      .isHighlighted = Process_rowIsHighlighted,
      .isVisible = Process_rowIsVisible,
      .matchesFilter = Process_rowMatchesFilter,
      .compareByParent = Process_compareByParent,
      .sortKeyString = Process_rowGetSortKey,
      .writeField = FreeBSDProcess_rowWriteField
   },
   .compareByKey = FreeBSDProcess_compareByKey
};
