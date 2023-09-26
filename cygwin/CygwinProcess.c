/*
htop - CygwinProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "cygwin/CygwinProcess.h"

#include <stdlib.h>

#include "CRT.h"
#include "Process.h"
#include "RichString.h"
#include "XUtils.h"
#include "cygwin/CygwinMachine.h"


const ProcessFieldData Process_fields[LAST_PROCESSFIELD] = {
   [0] = {
      .name = "",
      .title = NULL,
      .description = NULL,
      .flags = 0,
   },
   [PID] = {
      .name = "PID",
      .title = "PID",
      .description = "Process/thread ID",
      .flags = 0,
      .pidColumn = true,
   },
   [COMM] = {
      .name = "Command",
      .title = "Command ",
      .description = "Command line",
      .flags = 0,
   },
   [STATE] = {
      .name = "STATE",
      .title = "S ",
      .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)",
      .flags = 0,
   },
   [PPID] = {
      .name = "PPID",
      .title = "PPID",
      .description = "Parent process ID",
      .flags = 0,
      .pidColumn = true,
   },
   [PGRP] = {
      .name = "PGRP",
      .title = "PGRP",
      .description = "Process group ID",
      .flags = 0,
      .pidColumn = true,
   },
   [SESSION] = {
      .name = "SESSION",
      .title = "SESN",
      .description = "Process's session ID",
      .flags = 0,
      .pidColumn = true,
   },
   [TTY] = {
      .name = "TTY",
      .title = "TTY      ",
      .description = "Controlling terminal",
      .flags = 0,
   },
   [TPGID] = {
      .name = "TPGID",
      .title = "TPGID",
      .description = "Process ID of the fg process group of the controlling terminal",
      .flags = 0,
      .pidColumn = true,
   },
   [MINFLT] = {
      .name = "MINFLT",
      .title = "     MINFLT ",
      .description = "Number of minor faults which have not required loading a memory page from disk",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [CMINFLT] = {
      .name = "CMINFLT",
      .title = "    CMINFLT ",
      .description = "Children processes' minor faults",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [MAJFLT] = {
      .name = "MAJFLT",
      .title = "     MAJFLT ",
      .description = "Number of major faults which have required loading a memory page from disk",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [CMAJFLT] = {
      .name = "CMAJFLT",
      .title = "    CMAJFLT ",
      .description = "Children processes' major faults",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [UTIME] = {
      .name = "UTIME",
      .title = " UTIME+  ",
      .description = "User CPU time - time the process spent executing in user mode",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [STIME] = {
      .name = "STIME",
      .title = " STIME+  ",
      .description = "System CPU time - time the kernel spent running system calls for this process",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [CUTIME] = {
      .name = "CUTIME",
      .title = " CUTIME+ ",
      .description = "Children processes' user CPU time",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [CSTIME] = {
      .name = "CSTIME",
      .title = " CSTIME+ ",
      .description = "Children processes' system CPU time",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [PRIORITY] = {
      .name = "PRIORITY",
      .title = "PRI ",
      .description = "Kernel's internal priority for the process",
      .flags = 0,
   },
   [NICE] = {
      .name = "NICE",
      .title = " NI ",
      .description = "Nice value (the higher the value, the more it lets other processes take priority)",
      .flags = 0,
   },
   [STARTTIME] = {
      .name = "STARTTIME",
      .title = "START ",
      .description = "Time the process was started",
      .flags = 0,
   },
   [ELAPSED] = {
      .name = "ELAPSED",
      .title = "ELAPSED  ",
      .description = "Time since the process was started",
      .flags = 0,
   },
   [PROCESSOR] = {
      .name = "PROCESSOR",
      .title = "CPU ",
      .description = "Id of the CPU the process last executed on",
      .flags = 0,
   },
   [M_VIRT] = {
      .name = "M_VIRT",
      .title = " VIRT ",
      .description = "Total program size in virtual memory",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [M_RESIDENT] = {
      .name = "M_RESIDENT",
      .title = "  RES ",
      .description = "Resident set size, size of the text and data sections, plus stack usage",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [M_SHARE] = {
      .name = "M_SHARE",
      .title = "  SHR ",
      .description = "Size of the process's shared pages",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [M_TRS] = {
      .name = "M_TRS",
      .title = " CODE ",
      .description = "Size of the .text segment of the process (CODE)",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [M_DRS] = {
      .name = "M_DRS",
      .title = " DATA ",
      .description = "Size of the .data segment plus stack usage of the process (DATA)",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [M_LRS] = {
      .name = "M_LRS",
      .title = "  LIB ",
      .description = "The library size of the process",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [ST_UID] = {
      .name = "ST_UID",
      .title = "UID",
      .description = "User ID of the process owner",
      .flags = 0,
   },
   [PERCENT_CPU] = {
      .name = "PERCENT_CPU",
      .title = " CPU%",
      .description = "Percentage of the CPU time the process used in the last sampling",
      .flags = 0,
      .defaultSortDesc = true,
      .autoWidth = true,
   },
   [PERCENT_NORM_CPU] = {
      .name = "PERCENT_NORM_CPU",
      .title = "NCPU%",
      .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)",
      .flags = 0,
      .defaultSortDesc = true,
      .autoWidth = true,
   },
   [PERCENT_MEM] = {
      .name = "PERCENT_MEM",
      .title = "MEM% ",
      .description = "Percentage of the memory the process is using, based on resident memory size",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [USER] = {
      .name = "USER",
      .title = "USER       ",
      .description = "Username of the process owner (or user ID if name cannot be determined)",
      .flags = 0,
   },
   [TIME] = {
      .name = "TIME",
      .title = "  TIME+  ",
      .description = "Total time the process has spent in user and system time",
      .flags = 0,
      .defaultSortDesc = true,
   },
   [NLWP] = {
      .name = "NLWP",
      .title = "NLWP ",
      .description = "Number of threads in the process",
      .flags = 0,
   },
   [TGID] = {
      .name = "TGID",
      .title = "TGID",
      .description = "Thread group ID (i.e. process ID)",
      .flags = 0,
      .pidColumn = true,
   },
   [PROC_COMM] = {
      .name = "COMM",
      .title = "COMM            ",
      .description = "comm string of the process",
      .flags = 0,
   },
   [PROC_EXE] = {
      .name = "EXE",
      .title = "EXE             ",
      .description = "Basename of exe of the process from /proc/[pid]/exe",
      .flags = 0,
   },
   [CWD] = {
      .name = "CWD",
      .title = "CWD                       ",
      .description = "The current working directory of the process",
      .flags = PROCESS_FLAG_CWD,
   },

};

Process* CygwinProcess_new(const Machine* host) {
   CygwinProcess* this = xCalloc(1, sizeof(CygwinProcess));
   Object_setClass(this, Class(CygwinProcess));
   Process_init(&this->super, host);
   return &this->super;
}

void Process_delete(Object* cast) {
   CygwinProcess* this = (CygwinProcess*) cast;
   Process_done((Process*)cast);
   free(this);
}

static void CygwinProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const Process* this = (const Process*) super;
   const Machine* host = (const Machine*) super->host;
   const CygwinMachine* chost = (const CygwinMachine*) host;
   const CygwinProcess* cp = (const CygwinProcess*) this;
   bool coloring = host->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch (field) {
   case CMINFLT: Row_printCount(str, cp->cminflt, coloring); return;
   case CMAJFLT: Row_printCount(str, cp->cmajflt, coloring); return;
   case M_DRS: Row_printBytes(str, cp->m_drs * chost->pageSize, coloring); return;
   case M_LRS:
      if (cp->m_lrs) {
         Row_printBytes(str, cp->m_lrs * chost->pageSize, coloring);
         return;
      }

      attr = CRT_colors[PROCESS_SHADOW];
      xSnprintf(buffer, n, "  N/A ");
      break;
   case M_TRS: Row_printBytes(str, cp->m_trs * chost->pageSize, coloring); return;
   case M_SHARE: Row_printBytes(str, cp->m_share * chost->pageSize, coloring); return;
   case UTIME: Row_printTime(str, cp->utime, coloring); return;
   case STIME: Row_printTime(str, cp->stime, coloring); return;
   case CUTIME: Row_printTime(str, cp->cutime, coloring); return;
   case CSTIME: Row_printTime(str, cp->cstime, coloring); return;
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_appendWide(str, attr, buffer);
}

static int CygwinProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const CygwinProcess* p1 = (const CygwinProcess*)v1;
   const CygwinProcess* p2 = (const CygwinProcess*)v2;

   switch (key) {
   case M_DRS:
      return SPACESHIP_NUMBER(p1->m_drs, p2->m_drs);
   case M_LRS:
      return SPACESHIP_NUMBER(p1->m_lrs, p2->m_lrs);
   case M_TRS:
      return SPACESHIP_NUMBER(p1->m_trs, p2->m_trs);
   case M_SHARE:
      return SPACESHIP_NUMBER(p1->m_share, p2->m_share);
   case UTIME:
      return SPACESHIP_NUMBER(p1->utime, p2->utime);
   case CUTIME:
      return SPACESHIP_NUMBER(p1->cutime, p2->cutime);
   case STIME:
      return SPACESHIP_NUMBER(p1->stime, p2->stime);
   case CSTIME:
      return SPACESHIP_NUMBER(p1->cstime, p2->cstime);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

const ProcessClass CygwinProcess_class = {
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
      .writeField = CygwinProcess_rowWriteField
   },
   .compareByKey = CygwinProcess_compareByKey
};
