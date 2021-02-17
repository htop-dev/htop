/*
htop - PCPProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020 htop dev team
(C) 2020-2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "PCPProcess.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include "CRT.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "XUtils.h"

/* Used to identify kernel threads in Comm column */
static const char *const kthreadID = "KTHREAD";

const ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging, I idle)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "TPGID", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   [CMINFLT] = { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, .defaultSortDesc = true, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   [CMAJFLT] = { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, .defaultSortDesc = true, },
   [UTIME] = { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, .defaultSortDesc = true, },
   [STIME] = { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, .defaultSortDesc = true, },
   [CUTIME] = { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, .defaultSortDesc = true, },
   [CSTIME] = { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, .defaultSortDesc = true, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "If of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, .defaultSortDesc = true, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, .defaultSortDesc = true, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, .defaultSortDesc = true, },
   [M_LRS] = { .name = "M_LRS", .title = "  LIB ", .description = "The library size of the process (unused since Linux 2.6; always 0)", .flags = 0, .defaultSortDesc = true, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process (unused since Linux 2.6; always 0)", .flags = 0, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "  UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, .defaultSortDesc = true, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, .defaultSortDesc = true, },
   [TGID] = { .name = "TGID", .title = "TGID", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [RCHAR] = { .name = "RCHAR", .title = "RCHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [WCHAR] = { .name = "WCHAR", .title = "WCHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [SYSCR] = { .name = "SYSCR", .title = "  READ_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [SYSCW] = { .name = "SYSCW", .title = " WRITE_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [RBYTES] = { .name = "RBYTES", .title = " IO_R ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [WBYTES] = { .name = "WBYTES", .title = " IO_W ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [CNCLWB] = { .name = "CNCLWB", .title = " IO_C ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_READ_RATE] = { .name = "IO_READ_RATE", .title = " DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_WRITE_RATE] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_RATE] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [CGROUP] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
   [OOM] = { .name = "OOM", .title = " OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, .defaultSortDesc = true, },
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = "IOD% ", .description = "Block I/O delay %", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = 0, .defaultSortDesc = true, },
   [M_PSS] = { .name = "M_PSS", .title = "  PSS ", .description = "proportional set size, same as M_RESIDENT but each page is divided by the number of processes sharing it.", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_SWAP] = { .name = "M_SWAP", .title = " SWAP ", .description = "Size of the process's swapped pages", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_PSSWP] = { .name = "M_PSSWP", .title = " PSSWP ", .description = "shows proportional swap share of this mapping, Unlike \"Swap\", this does not take into account swapped out page of underlying shmem objects.", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [CTXT] = { .name = "CTXT", .title = " CTXT ", .description = "Context switches (incremental sum of voluntary_ctxt_switches and nonvoluntary_ctxt_switches)", .flags = PROCESS_FLAG_LINUX_CTXT, .defaultSortDesc = true, },
   [SECATTR] = { .name = "SECATTR", .title = " Security Attribute ", .description = "Security attribute of the process (e.g. SELinux or AppArmor)", .flags = PROCESS_FLAG_LINUX_SECATTR, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process from /proc/[pid]/comm", .flags = 0, },
};

/* This function returns the string displayed in Command column, so that sorting
 * happens on what is displayed - whether comm, full path, basename, etc.. So
 * this follows PCPProcess_writeField(COMM) and PCPProcess_writeCommand */
static const char* PCPProcess_getCommandStr(const Process *this) {
   const PCPProcess *pp = (const PCPProcess *)this;
   if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !pp->mergedCommand.str) {
      return this->comm;
   }
   return pp->mergedCommand.str;
}

Process* PCPProcess_new(const Settings* settings) {
   PCPProcess* this = xCalloc(1, sizeof(PCPProcess));
   Object_setClass(this, Class(PCPProcess));
   Process_init(&this->super, settings);
   return &this->super;
}

void Process_delete(Object* cast) {
   PCPProcess* this = (PCPProcess*) cast;
   Process_done((Process*)cast);
   free(this->cgroup);
   free(this->secattr);
   free(this->ttyDevice);
   free(this->procComm);
   free(this->mergedCommand.str);
   free(this);
}

static void PCPProcess_printDelay(float delay_percent, char* buffer, int n) {
   if (isnan(delay_percent)) {
      xSnprintf(buffer, n, " N/A  ");
   } else {
      xSnprintf(buffer, n, "%4.1f  ", delay_percent);
   }
}

/*
TASK_COMM_LEN is defined to be 16 for /proc/[pid]/comm in man proc(5), but is
not available in an userspace header - so define it. Note: when colorizing a
basename with the comm prefix, the entire basename (not just the comm prefix)
is colorized for better readability, and it is implicit that only up to
(TASK_COMM_LEN - 1) could be comm.
*/
#define TASK_COMM_LEN 16

/*
This function makes the merged Command string. It also stores the offsets of
the basename, comm w.r.t the merged Command string - these offsets will be used
by PCPProcess_writeCommand() for coloring. The merged Command string is also
returned by PCPProcess_getCommandStr() for searching, sorting and filtering.
*/
void PCPProcess_makeCommandStr(Process* this) {
   PCPProcess *pp = (PCPProcess *)this;
   PCPProcessMergedCommand *mc = &pp->mergedCommand;

   bool showMergedCommand = this->settings->showMergedCommand;
   bool showProgramPath = this->settings->showProgramPath;
   bool searchCommInCmdline = this->settings->findCommInCmdline;

   /* pp->mergedCommand.str needs updating only if its state or contents
    * changed.  Its content is based on the fields cmdline and comm. */
   if (
      mc->prevMergeSet == showMergedCommand &&
      mc->prevPathSet == showProgramPath &&
      mc->prevCommSet == searchCommInCmdline &&
      !mc->cmdlineChanged &&
      !mc->commChanged
   ) {
      return;
   }

   /* The field separtor "│" has been chosen such that it will not match any
    * valid string used for searching or filtering */
   const char *SEPARATOR = CRT_treeStr[TREE_STR_VERT];
   const int SEPARATOR_LEN = strlen(SEPARATOR);

   /* Check for any changed fields since we last built this string */
   if (mc->cmdlineChanged || mc->commChanged) {
      free(mc->str);
      /* Accommodate the column text, two field separators and terminating NUL */
      mc->str = xCalloc(1, mc->maxLen + 2*SEPARATOR_LEN + 1);
   }

   /* Preserve the settings used in this run */
   mc->prevMergeSet = showMergedCommand;
   mc->prevPathSet = showProgramPath;
   mc->prevCommSet = searchCommInCmdline;

   /* Mark everything as unchanged */
   mc->cmdlineChanged = false;
   mc->commChanged = false;

   /* Clear any separators */
   mc->sep1 = 0;
   mc->sep2 = 0;
   /* Clear any highlighting locations */
   mc->baseStart = 0;
   mc->baseEnd = 0;
   mc->commStart = 0;
   mc->commEnd = 0;

   const char *cmdline = this->comm;
   const char *procComm = pp->procComm;

   char *strStart = mc->str;
   char *str = strStart;

   int cmdlineBasenameOffset = pp->procCmdlineBasenameOffset;
   int cmdlineBasenameEnd = pp->procCmdlineBasenameEnd;

   if (!cmdline) {
      cmdlineBasenameOffset = 0;
      cmdlineBasenameEnd = 0;
      cmdline = "(zombie)";
   }

   assert(cmdlineBasenameOffset >= 0);
   assert(cmdlineBasenameOffset <= (int)strlen(cmdline));

   if (showMergedCommand && procComm && strlen(procComm)) {   /* Prefix column with comm */
      if (strncmp(cmdline + cmdlineBasenameOffset, procComm, MINIMUM(TASK_COMM_LEN - 1, strlen(procComm))) != 0) {
         mc->commStart = 0;
         mc->commEnd = strlen(procComm);

         str = stpcpy(str, procComm);

         mc->sep1 = str - strStart;
         str = stpcpy(str, SEPARATOR);
      }
   }

   if (showProgramPath) {
      (void) stpcpy(str, cmdline);
      mc->baseStart = cmdlineBasenameOffset;
      mc->baseEnd = cmdlineBasenameEnd;
   } else {
      (void) stpcpy(str, cmdline + cmdlineBasenameOffset);
      mc->baseStart = 0;
      mc->baseEnd = cmdlineBasenameEnd - cmdlineBasenameOffset;
   }

   if (mc->sep1) {
      mc->baseStart += str - strStart - SEPARATOR_LEN + 1;
      mc->baseEnd += str - strStart - SEPARATOR_LEN + 1;
   }
}

static void PCPProcess_writeCommand(const Process* this, int attr, int baseAttr, RichString* str) {
   const PCPProcess *pp = (const PCPProcess *)this;
   const PCPProcessMergedCommand *mc = &pp->mergedCommand;

   int strStart = RichString_size(str);

   int baseStart = strStart + pp->mergedCommand.baseStart;
   int baseEnd = strStart + pp->mergedCommand.baseEnd;
   int commStart = strStart + pp->mergedCommand.commStart;
   int commEnd = strStart + pp->mergedCommand.commEnd;

   int commAttr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];

   bool highlightBaseName = this->settings->highlightBaseName;

   RichString_appendWide(str, attr, pp->mergedCommand.str);

   if (pp->mergedCommand.commEnd) {
      if (!pp->mergedCommand.separateComm && commStart == baseStart && highlightBaseName) {
         /* If it was matched with binaries basename, make it bold if needed */
         if (commEnd > baseEnd) {
            RichString_setAttrn(str, A_BOLD | baseAttr, baseStart, baseEnd - baseStart);
            RichString_setAttrn(str, A_BOLD | commAttr, baseEnd, commEnd - baseEnd);
         } else if (commEnd < baseEnd) {
            RichString_setAttrn(str, A_BOLD | commAttr, commStart, commEnd - commStart);
            RichString_setAttrn(str, A_BOLD | baseAttr, commEnd, baseEnd - commEnd);
         } else {
            // Actually should be highlighted commAttr, but marked baseAttr to reduce visual noise
            RichString_setAttrn(str, A_BOLD | baseAttr, commStart, commEnd - commStart);
         }

         baseStart = baseEnd;
      } else {
         RichString_setAttrn(str, commAttr, commStart, commEnd - commStart);
      }
   }

   if (baseStart < baseEnd && highlightBaseName) {
      RichString_setAttrn(str, baseAttr, baseStart, baseEnd - baseStart);
   }

   if (mc->sep1)
      RichString_setAttrn(str, CRT_colors[FAILED_READ], strStart + mc->sep1, 1);
   if (mc->sep2)
      RichString_setAttrn(str, CRT_colors[FAILED_READ], strStart + mc->sep2, 1);
}

static void PCPProcess_writeCommandField(const Process *this, RichString *str, char *buffer, int n, int attr) {
   /* This code is from Process_writeField for COMM, but we invoke
    * PCPProcess_writeCommand to display the full binary path
    * (or its basename)│/proc/pid/comm│/proc/pid/cmdline */
   int baseattr = CRT_colors[PROCESS_BASENAME];
   if (this->settings->highlightThreads && Process_isThread(this)) {
      attr = CRT_colors[PROCESS_THREAD];
      baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
   }
   if (!this->settings->treeView || this->indent == 0) {
      PCPProcess_writeCommand(this, attr, baseattr, str);
   } else {
      char* buf = buffer;
      int maxIndent = 0;
      bool lastItem = (this->indent < 0);
      int indent = (this->indent < 0 ? -this->indent : this->indent);
      int vertLen = strlen(CRT_treeStr[TREE_STR_VERT]);

      for (int i = 0; i < 32; i++) {
         if (indent & (1U << i)) {
            maxIndent = i+1;
         }
      }
      for (int i = 0; i < maxIndent - 1; i++) {
         if (indent & (1 << i)) {
            if (buf - buffer + (vertLen + 3) > n) {
               break;
            }
            buf = stpcpy(buf, CRT_treeStr[TREE_STR_VERT]);
            buf = stpcpy(buf, "  ");
         } else {
            if (buf - buffer + 4 > n) {
               break;
            }
            buf = stpcpy(buf, "   ");
         }
      }

      n -= (buf - buffer);
      const char* draw = CRT_treeStr[lastItem ? TREE_STR_BEND : TREE_STR_RTEE];
      xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
      RichString_appendWide(str, CRT_colors[PROCESS_TREE], buffer);
      PCPProcess_writeCommand(this, attr, baseattr, str);
   }
}

static void PCPProcess_writeField(const Process* this, RichString* str, ProcessField field) {
   const PCPProcess* pp = (const PCPProcess*) this;
   bool coloring = this->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int)field) {
   case TTY_NR:
      if (pp->ttyDevice) {
         xSnprintf(buffer, n, "%-8s", pp->ttyDevice + 5 /* skip "/dev/" */);
	 break;
      }
      Process_writeField(this, str, field);
      break;
   case CMINFLT: Process_colorNumber(str, pp->cminflt, coloring); return;
   case CMAJFLT: Process_colorNumber(str, pp->cmajflt, coloring); return;
   case M_DRS: Process_humanNumber(str, pp->m_drs, coloring); return;
   case M_DT: Process_humanNumber(str, pp->m_dt, coloring); return;
   case M_LRS: Process_humanNumber(str, pp->m_lrs, coloring); return;
   case M_TRS: Process_humanNumber(str, pp->m_trs, coloring); return;
   case M_SHARE: Process_humanNumber(str, pp->m_share, coloring); return;
   case M_PSS: Process_humanNumber(str, pp->m_pss, coloring); return;
   case M_SWAP: Process_humanNumber(str, pp->m_swap, coloring); return;
   case M_PSSWP: Process_humanNumber(str, pp->m_psswp, coloring); return;
   case UTIME: Process_printTime(str, pp->utime); return;
   case STIME: Process_printTime(str, pp->stime); return;
   case CUTIME: Process_printTime(str, pp->cutime); return;
   case CSTIME: Process_printTime(str, pp->cstime); return;
   case RCHAR:  Process_humanNumber(str, pp->io_rchar, coloring); return;
   case WCHAR:  Process_humanNumber(str, pp->io_wchar, coloring); return;
   case SYSCR:  Process_colorNumber(str, pp->io_syscr, coloring); return;
   case SYSCW:  Process_colorNumber(str, pp->io_syscw, coloring); return;
   case RBYTES: Process_humanNumber(str, pp->io_read_bytes, coloring); return;
   case WBYTES: Process_humanNumber(str, pp->io_write_bytes, coloring); return;
   case CNCLWB: Process_humanNumber(str, pp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Process_outputRate(str, buffer, n, pp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_outputRate(str, buffer, n, pp->io_rate_write_bps, coloring); return;
   case IO_RATE: {
      double totalRate = NAN;
      if (!isnan(pp->io_rate_read_bps) && !isnan(pp->io_rate_write_bps))
         totalRate = pp->io_rate_read_bps + pp->io_rate_write_bps;
      else if (!isnan(pp->io_rate_read_bps))
         totalRate = pp->io_rate_read_bps;
      else if (!isnan(pp->io_rate_write_bps))
         totalRate = pp->io_rate_write_bps;
      else
         totalRate = NAN;
      Process_outputRate(str, buffer, n, totalRate, coloring); return;
   }
   case CGROUP: xSnprintf(buffer, n, "%-10s ", pp->cgroup ? pp->cgroup : ""); break;
   case OOM: xSnprintf(buffer, n, "%4u ", pp->oom); break;
   case PERCENT_CPU_DELAY:
      PCPProcess_printDelay(pp->cpu_delay_percent, buffer, n);
      break;
   case PERCENT_IO_DELAY:
      PCPProcess_printDelay(pp->blkio_delay_percent, buffer, n);
      break;
   case PERCENT_SWAP_DELAY:
      PCPProcess_printDelay(pp->swapin_delay_percent, buffer, n);
      break;
   case CTXT:
      if (pp->ctxt_diff > 1000) {
         attr |= A_BOLD;
      }
      xSnprintf(buffer, n, "%5lu ", pp->ctxt_diff);
      break;
   case SECATTR: snprintf(buffer, n, "%-30s   ", pp->secattr ? pp->secattr : "?"); break;
   case COMM: {
      if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !pp->mergedCommand.str) {
         Process_writeField(this, str, field);
      } else {
         PCPProcess_writeCommandField(this, str, buffer, n, attr);
      }
      return;
   }
   case PROC_COMM: {
      const char* procComm;
      if (pp->procComm) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];
         procComm = pp->procComm;
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         procComm = Process_isKernelThread(this) ? kthreadID : "N/A";
      }
      /* 15 being (TASK_COMM_LEN - 1) */
      Process_printLeftAlignedField(str, attr, procComm, 15);
      return;
   }
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_appendWide(str, attr, buffer);
}

static double adjustNaN(double num) {
   if (isnan(num))
      return -0.0005;

   return num;
}

static int PCPProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const PCPProcess* p1 = (const PCPProcess*)v1;
   const PCPProcess* p2 = (const PCPProcess*)v2;

   switch (key) {
   case M_DRS:
      return SPACESHIP_NUMBER(p1->m_drs, p2->m_drs);
   case M_DT:
      return SPACESHIP_NUMBER(p1->m_dt, p2->m_dt);
   case M_LRS:
      return SPACESHIP_NUMBER(p1->m_lrs, p2->m_lrs);
   case M_TRS:
      return SPACESHIP_NUMBER(p1->m_trs, p2->m_trs);
   case M_SHARE:
      return SPACESHIP_NUMBER(p1->m_share, p2->m_share);
   case M_PSS:
      return SPACESHIP_NUMBER(p1->m_pss, p2->m_pss);
   case M_SWAP:
      return SPACESHIP_NUMBER(p1->m_swap, p2->m_swap);
   case M_PSSWP:
      return SPACESHIP_NUMBER(p1->m_psswp, p2->m_psswp);
   case UTIME:
      return SPACESHIP_NUMBER(p1->utime, p2->utime);
   case CUTIME:
      return SPACESHIP_NUMBER(p1->cutime, p2->cutime);
   case STIME:
      return SPACESHIP_NUMBER(p1->stime, p2->stime);
   case CSTIME:
      return SPACESHIP_NUMBER(p1->cstime, p2->cstime);
   case RCHAR:
      return SPACESHIP_NUMBER(p1->io_rchar, p2->io_rchar);
   case WCHAR:
      return SPACESHIP_NUMBER(p1->io_wchar, p2->io_wchar);
   case SYSCR:
      return SPACESHIP_NUMBER(p1->io_syscr, p2->io_syscr);
   case SYSCW:
      return SPACESHIP_NUMBER(p1->io_syscw, p2->io_syscw);
   case RBYTES:
      return SPACESHIP_NUMBER(p1->io_read_bytes, p2->io_read_bytes);
   case WBYTES:
      return SPACESHIP_NUMBER(p1->io_write_bytes, p2->io_write_bytes);
   case CNCLWB:
      return SPACESHIP_NUMBER(p1->io_cancelled_write_bytes, p2->io_cancelled_write_bytes);
   case IO_READ_RATE:
      return SPACESHIP_NUMBER(adjustNaN(p1->io_rate_read_bps), adjustNaN(p2->io_rate_read_bps));
   case IO_WRITE_RATE:
      return SPACESHIP_NUMBER(adjustNaN(p1->io_rate_write_bps), adjustNaN(p2->io_rate_write_bps));
   case IO_RATE:
      return SPACESHIP_NUMBER(adjustNaN(p1->io_rate_read_bps) + adjustNaN(p1->io_rate_write_bps), adjustNaN(p2->io_rate_read_bps) + adjustNaN(p2->io_rate_write_bps));
   case CGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup, p2->cgroup);
   case OOM:
      return SPACESHIP_NUMBER(p1->oom, p1->oom);
   case PERCENT_CPU_DELAY:
      return SPACESHIP_NUMBER(p1->cpu_delay_percent, p1->cpu_delay_percent);
   case PERCENT_IO_DELAY:
      return SPACESHIP_NUMBER(p1->blkio_delay_percent, p1->blkio_delay_percent);
   case PERCENT_SWAP_DELAY:
      return SPACESHIP_NUMBER(p1->swapin_delay_percent, p1->swapin_delay_percent);
   case CTXT:
      return SPACESHIP_NUMBER(p1->ctxt_diff, p1->ctxt_diff);
   case SECATTR:
      return SPACESHIP_NULLSTR(p1->secattr, p2->secattr);
   case PROC_COMM: {
      const char *comm1 = p1->procComm ? p1->procComm : (Process_isKernelThread(v1) ? kthreadID : "");
      const char *comm2 = p2->procComm ? p2->procComm : (Process_isKernelThread(v2) ? kthreadID : "");
      return strcmp(comm1, comm2);
   }
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

bool Process_isThread(const Process* this) {
   return (Process_isUserlandThread(this) || Process_isKernelThread(this));
}

const ProcessClass PCPProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = PCPProcess_writeField,
   .getCommandStr = PCPProcess_getCommandStr,
   .compareByKey = PCPProcess_compareByKey
};
