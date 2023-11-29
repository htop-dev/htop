/*
htop - PCPProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPProcess.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/PCPDynamicColumn.h"


const ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging, I idle)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, },
   [TTY] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
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
   [ELAPSED] = { .name = "ELAPSED", .title = "ELAPSED  ", .description = "Time since the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "If of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, .defaultSortDesc = true, },
   [M_PRIV] = { .name = "M_PRIV", .title = " PRIV ", .description = "The private memory size of the process - resident set size minus shared memory", .flags = 0, .defaultSortDesc = true, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, .defaultSortDesc = true, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, .defaultSortDesc = true, },
   [M_LRS] = { .name = "M_LRS", .title = "  LIB ", .description = "The library size of the process (unused since Linux 2.6; always 0)", .flags = 0, .defaultSortDesc = true, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process (unused since Linux 2.6; always 0)", .flags = 0, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "UID", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = " CPU%", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER       ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
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
   [CGROUP] = { .name = "CGROUP", .title = "CGROUP (raw)                        ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
   [CCGROUP] = { .name = "CCGROUP", .title = "CGROUP (compressed)                 ", .description = "Which cgroup the process is in (condensed to essentials)", .flags = PROCESS_FLAG_LINUX_CGROUP, },
   [CONTAINER] = { .name = "CONTAINER", .title = "CONTAINER                           ", .description = "Name of the container the process is in (guessed by heuristics)", .flags = PROCESS_FLAG_LINUX_CGROUP, },
   [OOM] = { .name = "OOM", .title = " OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, .defaultSortDesc = true, },
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = " IOD% ", .description = "Block I/O delay %", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = 0, .defaultSortDesc = true, },
   [M_PSS] = { .name = "M_PSS", .title = "  PSS ", .description = "proportional set size, same as M_RESIDENT but each page is divided by the number of processes sharing it.", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_SWAP] = { .name = "M_SWAP", .title = " SWAP ", .description = "Size of the process's swapped pages", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_PSSWP] = { .name = "M_PSSWP", .title = " PSSWP ", .description = "shows proportional swap share of this mapping, Unlike \"Swap\", this does not take into account swapped out page of underlying shmem objects.", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [CTXT] = { .name = "CTXT", .title = " CTXT ", .description = "Context switches (incremental sum of voluntary_ctxt_switches and nonvoluntary_ctxt_switches)", .flags = PROCESS_FLAG_LINUX_CTXT, .defaultSortDesc = true, },
   [SECATTR] = { .name = "SECATTR", .title = " Security Attribute ", .description = "Security attribute of the process (e.g. SELinux or AppArmor)", .flags = PROCESS_FLAG_LINUX_SECATTR, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process", .flags = 0, },
   [CWD] = { .name = "CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_CWD, },
   [AUTOGROUP_ID] = { .name = "AUTOGROUP_ID", .title = "AGRP", .description = "The autogroup identifier of the process", .flags = PROCESS_FLAG_LINUX_AUTOGROUP, },
   [AUTOGROUP_NICE] = { .name = "AUTOGROUP_NICE", .title = " ANI", .description = "Nice value (the higher the value, the more other processes take priority) associated with the process autogroup", .flags = PROCESS_FLAG_LINUX_AUTOGROUP, },
};

Process* PCPProcess_new(const Machine* host) {
   PCPProcess* this = xCalloc(1, sizeof(PCPProcess));
   Object_setClass(this, Class(PCPProcess));
   Process_init(&this->super, host);
   return &this->super;
}

void Process_delete(Object* cast) {
   PCPProcess* this = (PCPProcess*) cast;
   Process_done((Process*)cast);
   free(this->cgroup_short);
   free(this->cgroup);
   free(this->secattr);
   free(this);
}

static void PCPProcess_printDelay(float delay_percent, char* buffer, size_t n) {
   if (isNonnegative(delay_percent)) {
      xSnprintf(buffer, n, "%4.1f  ", delay_percent);
   } else {
      xSnprintf(buffer, n, " N/A  ");
   }
}

static double PCPProcess_totalIORate(const PCPProcess* pp) {
   double totalRate = NAN;
   if (isNonnegative(pp->io_rate_read_bps)) {
      totalRate = pp->io_rate_read_bps;
      if (isNonnegative(pp->io_rate_write_bps)) {
         totalRate += pp->io_rate_write_bps;
      }
   } else if (isNonnegative(pp->io_rate_write_bps)) {
      totalRate = pp->io_rate_write_bps;
   }
   return totalRate;
}

static void PCPProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const PCPProcess* pp = (const PCPProcess*) super;

   bool coloring = super->host->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;

   switch ((int)field) {
   case CMINFLT: Row_printCount(str, pp->cminflt, coloring); return;
   case CMAJFLT: Row_printCount(str, pp->cmajflt, coloring); return;
   case M_DRS: Row_printBytes(str, pp->m_drs, coloring); return;
   case M_DT: Row_printBytes(str, pp->m_dt, coloring); return;
   case M_LRS: Row_printBytes(str, pp->m_lrs, coloring); return;
   case M_TRS: Row_printBytes(str, pp->m_trs, coloring); return;
   case M_SHARE: Row_printBytes(str, pp->m_share, coloring); return;
   case M_PRIV: Row_printBytes(str, pp->m_priv, coloring); return;
   case M_PSS: Row_printKBytes(str, pp->m_pss, coloring); return;
   case M_SWAP: Row_printKBytes(str, pp->m_swap, coloring); return;
   case M_PSSWP: Row_printKBytes(str, pp->m_psswp, coloring); return;
   case UTIME: Row_printTime(str, pp->utime, coloring); return;
   case STIME: Row_printTime(str, pp->stime, coloring); return;
   case CUTIME: Row_printTime(str, pp->cutime, coloring); return;
   case CSTIME: Row_printTime(str, pp->cstime, coloring); return;
   case RCHAR:  Row_printBytes(str, pp->io_rchar, coloring); return;
   case WCHAR:  Row_printBytes(str, pp->io_wchar, coloring); return;
   case SYSCR:  Row_printCount(str, pp->io_syscr, coloring); return;
   case SYSCW:  Row_printCount(str, pp->io_syscw, coloring); return;
   case RBYTES: Row_printBytes(str, pp->io_read_bytes, coloring); return;
   case WBYTES: Row_printBytes(str, pp->io_write_bytes, coloring); return;
   case CNCLWB: Row_printBytes(str, pp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Row_printRate(str, pp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Row_printRate(str, pp->io_rate_write_bps, coloring); return;
   case IO_RATE: Row_printRate(str, PCPProcess_totalIORate(pp), coloring); return;
   case CGROUP: xSnprintf(buffer, n, "%-35.35s ", pp->cgroup ? pp->cgroup : "N/A"); break;
   case CCGROUP: xSnprintf(buffer, n, "%-35.35s ", pp->cgroup_short ? pp->cgroup_short : (pp->cgroup ? pp->cgroup : "N/A")); break;
   case CONTAINER: xSnprintf(buffer, n, "%-35.35s ", pp->container_short ? pp->container_short : "N/A"); break;
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
   case AUTOGROUP_ID:
      if (pp->autogroup_id != -1) {
         xSnprintf(buffer, n, "%4ld ", pp->autogroup_id);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, " N/A ");
      }
      break;
   case AUTOGROUP_NICE:
      if (pp->autogroup_id != -1) {
         xSnprintf(buffer, n, "%3d ", pp->autogroup_nice);
         attr = pp->autogroup_nice < 0 ? CRT_colors[PROCESS_HIGH_PRIORITY]
              : pp->autogroup_nice > 0 ? CRT_colors[PROCESS_LOW_PRIORITY]
              : CRT_colors[PROCESS_SHADOW];
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "N/A ");
      }
      break;
   default:
      Process_writeField(&pp->super, str, field);
      return;
   }

   RichString_appendWide(str, attr, buffer);
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
   case M_PRIV:
      return SPACESHIP_NUMBER(p1->m_priv, p2->m_priv);
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
      return compareRealNumbers(p1->io_rate_read_bps, p2->io_rate_read_bps);
   case IO_WRITE_RATE:
      return compareRealNumbers(p1->io_rate_write_bps, p2->io_rate_write_bps);
   case IO_RATE:
      return compareRealNumbers(PCPProcess_totalIORate(p1), PCPProcess_totalIORate(p2));
   case CGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup, p2->cgroup);
   case CCGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup_short, p2->cgroup_short);
   case CONTAINER:
      return SPACESHIP_NULLSTR(p1->container_short, p2->container_short);
   case OOM:
      return SPACESHIP_NUMBER(p1->oom, p2->oom);
   case PERCENT_CPU_DELAY:
      return compareRealNumbers(p1->cpu_delay_percent, p2->cpu_delay_percent);
   case PERCENT_IO_DELAY:
      return compareRealNumbers(p1->blkio_delay_percent, p2->blkio_delay_percent);
   case PERCENT_SWAP_DELAY:
      return compareRealNumbers(p1->swapin_delay_percent, p2->swapin_delay_percent);
   case CTXT:
      return SPACESHIP_NUMBER(p1->ctxt_diff, p2->ctxt_diff);
   case SECATTR:
      return SPACESHIP_NULLSTR(p1->secattr, p2->secattr);
   case AUTOGROUP_ID:
      return SPACESHIP_NUMBER(p1->autogroup_id, p2->autogroup_id);
   case AUTOGROUP_NICE:
      return SPACESHIP_NUMBER(p1->autogroup_nice, p2->autogroup_nice);
   default:
      if (key < LAST_PROCESSFIELD)
         return Process_compareByKey_Base(v1, v2, key);
      return PCPDynamicColumn_compareByKey(p1, p2, key);
   }
}

const ProcessClass PCPProcess_class = {
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
      .writeField = PCPProcess_rowWriteField,
   },
   .compareByKey = PCPProcess_compareByKey,
};
