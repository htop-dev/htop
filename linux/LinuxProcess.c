/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "LinuxProcess.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>

/* semi-global */
long long btime;

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging, I idle)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [FLAGS] = { .name = "FLAGS", .title = NULL, .description = NULL, .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [CMINFLT] = { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [CMAJFLT] = { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
   [UTIME] = { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, },
   [STIME] = { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, },
   [CUTIME] = { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, },
   [CSTIME] = { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [ITREALVALUE] = { .name = "ITREALVALUE", .title = NULL, .description = NULL, .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [VSIZE] = { .name = "VSIZE", .title = NULL, .description = NULL, .flags = 0, },
   [RSS] = { .name = "RSS", .title = NULL, .description = NULL, .flags = 0, },
   [RLIM] = { .name = "RLIM", .title = NULL, .description = NULL, .flags = 0, },
   [STARTCODE] = { .name = "STARTCODE", .title = NULL, .description = NULL, .flags = 0, },
   [ENDCODE] = { .name = "ENDCODE", .title = NULL, .description = NULL, .flags = 0, },
   [STARTSTACK] = { .name = "STARTSTACK", .title = NULL, .description = NULL, .flags = 0, },
   [KSTKESP] = { .name = "KSTKESP", .title = NULL, .description = NULL, .flags = 0, },
   [KSTKEIP] = { .name = "KSTKEIP", .title = NULL, .description = NULL, .flags = 0, },
   [SIGNAL] = { .name = "SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [BLOCKED] = { .name = "BLOCKED", .title = NULL, .description = NULL, .flags = 0, },
   [SSIGIGNORE] = { .name = "SIGIGNORE", .title = NULL, .description = NULL, .flags = 0, },
   [SIGCATCH] = { .name = "SIGCATCH", .title = NULL, .description = NULL, .flags = 0, },
   [WCHAN] = { .name = "WCHAN", .title = NULL, .description = NULL, .flags = 0, },
   [NSWAP] = { .name = "NSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [CNSWAP] = { .name = "CNSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [EXIT_SIGNAL] = { .name = "EXIT_SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   [M_LRS] = { .name = "M_LRS", .title = " LIB ", .description = "The library size of the process", .flags = 0, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = "  UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   [CTID] = { .name = "CTID", .title = "   CTID ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   [VPID] = { .name = "VPID", .title = " VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   [VXID] = { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
#ifdef HAVE_TASKSTATS
   [RCHAR] = { .name = "RCHAR", .title = "    RD_CHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, },
   [WCHAR] = { .name = "WCHAR", .title = "    WR_CHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, },
   [SYSCR] = { .name = "SYSCR", .title = "    RD_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [SYSCW] = { .name = "SYSCW", .title = "    WR_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [RBYTES] = { .name = "RBYTES", .title = "  IO_RBYTES ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [WBYTES] = { .name = "WBYTES", .title = "  IO_WBYTES ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [CNCLWB] = { .name = "CNCLWB", .title = "  IO_CANCEL ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, },
   [IO_READ_RATE] = { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [IO_WRITE_RATE] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [IO_RATE] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, },
#endif
#ifdef HAVE_CGROUP
   [CGROUP] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
#endif
   [OOM] = { .name = "OOM", .title = " OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, },
   [IO_PRIORITY] = { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
#ifdef HAVE_DELAYACCT
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = 0, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = "IOD% ", .description = "Block I/O delay %", .flags = 0, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = 0, },
#endif
   [M_PSS] = { .name = "M_PSS", .title = "  PSS ", .description = "proportional set size, same as M_RESIDENT but each page is divided by the number of processes sharing it.", .flags = PROCESS_FLAG_LINUX_SMAPS, },
   [M_SWAP] = { .name = "M_SWAP", .title = " SWAP ", .description = "Size of the process's swapped pages", .flags = PROCESS_FLAG_LINUX_SMAPS, },
   [M_PSSWP] = { .name = "M_PSSWP", .title = " PSSWP ", .description = "shows proportional swap share of this mapping, Unlike \"Swap\", this does not take into account swapped out page of underlying shmem objects.", .flags = PROCESS_FLAG_LINUX_SMAPS, },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   #ifdef HAVE_OPENVZ
   { .id = VPID, .label = "VPID" },
   #endif
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = 0, .label = NULL },
};

ProcessClass LinuxProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = LinuxProcess_compare
   },
   .writeField = (Process_WriteField) LinuxProcess_writeField,
};

LinuxProcess* LinuxProcess_new(Settings* settings) {
   LinuxProcess* this = xCalloc(1, sizeof(LinuxProcess));
   Object_setClass(this, Class(LinuxProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
#ifdef HAVE_CGROUP
   free(this->cgroup);
#endif
   free(this->ttyDevice);
   free(this);
}

/*
[1] Note that before kernel 2.6.26 a process that has not asked for
an io priority formally uses "none" as scheduling class, but the
io scheduler will treat such processes as if it were in the best
effort class. The priority within the best effort class will  be
dynamically  derived  from  the  cpu  nice level of the process:
io_priority = (cpu_nice + 20) / 5. -- From ionice(1) man page
*/
#define LinuxProcess_effectiveIOPriority(p_) (IOPriority_class(p_->ioPriority) == IOPRIO_CLASS_NONE ? IOPriority_tuple(IOPRIO_CLASS_BE, (p_->super.nice + 20) / 5) : p_->ioPriority)

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this) {
   IOPriority ioprio = 0;
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_get
   ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->super.pid);
#endif
   this->ioPriority = ioprio;
   return ioprio;
}

bool LinuxProcess_setIOPriority(LinuxProcess* this, Arg ioprio) {
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_set
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->super.pid, ioprio.i);
#endif
   return (LinuxProcess_updateIOPriority(this) == ioprio.i);
}

#ifdef HAVE_DELAYACCT
void LinuxProcess_printDelay(float delay_percent, char* buffer, int n) {
  if (delay_percent == -1LL) {
    xSnprintf(buffer, n, " N/A  ");
  } else {
    xSnprintf(buffer, n, "%4.1f  ", delay_percent);
  }
}
#endif

void LinuxProcess_writeField(Process* this, RichString* str, ProcessField field) {
   LinuxProcess* lp = (LinuxProcess*) this;
   bool coloring = this->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch ((int)field) {
   case TTY_NR: {
      if (lp->ttyDevice) {
         xSnprintf(buffer, n, "%-9s", lp->ttyDevice + 5 /* skip "/dev/" */);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "?        ");
      }
      break;
   }
   case CMINFLT: Process_colorNumber(str, lp->cminflt, coloring); return;
   case CMAJFLT: Process_colorNumber(str, lp->cmajflt, coloring); return;
   case M_DRS: Process_humanNumber(str, lp->m_drs * PAGE_SIZE_KB, coloring); return;
   case M_DT: Process_humanNumber(str, lp->m_dt * PAGE_SIZE_KB, coloring); return;
   case M_LRS: Process_humanNumber(str, lp->m_lrs * PAGE_SIZE_KB, coloring); return;
   case M_TRS: Process_humanNumber(str, lp->m_trs * PAGE_SIZE_KB, coloring); return;
   case M_SHARE: Process_humanNumber(str, lp->m_share * PAGE_SIZE_KB, coloring); return;
   case M_PSS: Process_humanNumber(str, lp->m_pss, coloring); return;
   case M_SWAP: Process_humanNumber(str, lp->m_swap, coloring); return;
   case M_PSSWP: Process_humanNumber(str, lp->m_psswp, coloring); return;
   case UTIME: Process_printTime(str, lp->utime); return;
   case STIME: Process_printTime(str, lp->stime); return;
   case CUTIME: Process_printTime(str, lp->cutime); return;
   case CSTIME: Process_printTime(str, lp->cstime); return;
   case STARTTIME: {
     struct tm date;
     time_t starttimewall = btime + (lp->starttime / sysconf(_SC_CLK_TCK));
     (void) localtime_r(&starttimewall, &date);
     strftime(buffer, n, ((starttimewall > time(NULL) - 86400) ? "%R " : "%b%d "), &date);
     break;
   }
   #ifdef HAVE_TASKSTATS
   case RCHAR:  Process_colorNumber(str, lp->io_rchar, coloring); return;
   case WCHAR:  Process_colorNumber(str, lp->io_wchar, coloring); return;
   case SYSCR:  Process_colorNumber(str, lp->io_syscr, coloring); return;
   case SYSCW:  Process_colorNumber(str, lp->io_syscw, coloring); return;
   case RBYTES: Process_colorNumber(str, lp->io_read_bytes, coloring); return;
   case WBYTES: Process_colorNumber(str, lp->io_write_bytes, coloring); return;
   case CNCLWB: Process_colorNumber(str, lp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Process_outputRate(str, buffer, n, lp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_outputRate(str, buffer, n, lp->io_rate_write_bps, coloring); return;
   case IO_RATE: {
      double totalRate = (lp->io_rate_read_bps != -1)
                       ? (lp->io_rate_read_bps + lp->io_rate_write_bps)
                       : -1;
      Process_outputRate(str, buffer, n, totalRate, coloring); return;
   }
   #endif
   #ifdef HAVE_OPENVZ
   case CTID: xSnprintf(buffer, n, "%7u ", lp->ctid); break;
   case VPID: xSnprintf(buffer, n, Process_pidFormat, lp->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: xSnprintf(buffer, n, "%5u ", lp->vxid); break;
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP: xSnprintf(buffer, n, "%-10s ", lp->cgroup); break;
   #endif
   case OOM: xSnprintf(buffer, n, "%4u ", lp->oom); break;
   case IO_PRIORITY: {
      int klass = IOPriority_class(lp->ioPriority);
      if (klass == IOPRIO_CLASS_NONE) {
         // see note [1] above
         xSnprintf(buffer, n, "B%1d ", (int) (this->nice + 20) / 5);
      } else if (klass == IOPRIO_CLASS_BE) {
         xSnprintf(buffer, n, "B%1d ", IOPriority_data(lp->ioPriority));
      } else if (klass == IOPRIO_CLASS_RT) {
         attr = CRT_colors[PROCESS_HIGH_PRIORITY];
         xSnprintf(buffer, n, "R%1d ", IOPriority_data(lp->ioPriority));
      } else if (klass == IOPRIO_CLASS_IDLE) {
         attr = CRT_colors[PROCESS_LOW_PRIORITY];
         xSnprintf(buffer, n, "id ");
      } else {
         xSnprintf(buffer, n, "?? ");
      }
      break;
   }
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY: LinuxProcess_printDelay(lp->cpu_delay_percent, buffer, n); break;
   case PERCENT_IO_DELAY: LinuxProcess_printDelay(lp->blkio_delay_percent, buffer, n); break;
   case PERCENT_SWAP_DELAY: LinuxProcess_printDelay(lp->swapin_delay_percent, buffer, n); break;
   #endif
   default:
      Process_writeField((Process*)this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long LinuxProcess_compare(const void* v1, const void* v2) {
   LinuxProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (LinuxProcess*)v1;
      p2 = (LinuxProcess*)v2;
   } else {
      p2 = (LinuxProcess*)v1;
      p1 = (LinuxProcess*)v2;
   }
   long long diff;
   switch ((int)settings->sortKey) {
   case M_DRS:
      return (p2->m_drs - p1->m_drs);
   case M_DT:
      return (p2->m_dt - p1->m_dt);
   case M_LRS:
      return (p2->m_lrs - p1->m_lrs);
   case M_TRS:
      return (p2->m_trs - p1->m_trs);
   case M_SHARE:
      return (p2->m_share - p1->m_share);
   case M_PSS:
      return (p2->m_pss - p1->m_pss);
   case M_SWAP:
      return (p2->m_swap - p1->m_swap);
   case M_PSSWP:
      return (p2->m_psswp - p1->m_psswp);
   case UTIME:  diff = p2->utime - p1->utime; goto test_diff;
   case CUTIME: diff = p2->cutime - p1->cutime; goto test_diff;
   case STIME:  diff = p2->stime - p1->stime; goto test_diff;
   case CSTIME: diff = p2->cstime - p1->cstime; goto test_diff;
   case STARTTIME: {
      if (p1->starttime == p2->starttime)
         return (p1->super.pid - p2->super.pid);
      else
         return (p1->starttime - p2->starttime);
   }
   #ifdef HAVE_TASKSTATS
   case RCHAR:  diff = p2->io_rchar - p1->io_rchar; goto test_diff;
   case WCHAR:  diff = p2->io_wchar - p1->io_wchar; goto test_diff;
   case SYSCR:  diff = p2->io_syscr - p1->io_syscr; goto test_diff;
   case SYSCW:  diff = p2->io_syscw - p1->io_syscw; goto test_diff;
   case RBYTES: diff = p2->io_read_bytes - p1->io_read_bytes; goto test_diff;
   case WBYTES: diff = p2->io_write_bytes - p1->io_write_bytes; goto test_diff;
   case CNCLWB: diff = p2->io_cancelled_write_bytes - p1->io_cancelled_write_bytes; goto test_diff;
   case IO_READ_RATE:  diff = p2->io_rate_read_bps - p1->io_rate_read_bps; goto test_diff;
   case IO_WRITE_RATE: diff = p2->io_rate_write_bps - p1->io_rate_write_bps; goto test_diff;
   case IO_RATE: diff = (p2->io_rate_read_bps + p2->io_rate_write_bps) - (p1->io_rate_read_bps + p1->io_rate_write_bps); goto test_diff;
   #endif
   #ifdef HAVE_OPENVZ
   case CTID:
      return (p2->ctid - p1->ctid);
   case VPID:
      return (p2->vpid - p1->vpid);
   #endif
   #ifdef HAVE_VSERVER
   case VXID:
      return (p2->vxid - p1->vxid);
   #endif
   #ifdef HAVE_CGROUP
   case CGROUP:
      return strcmp(p1->cgroup ? p1->cgroup : "", p2->cgroup ? p2->cgroup : "");
   #endif
   case OOM:
      return ((int)p2->oom - (int)p1->oom);
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY:
      return (p2->cpu_delay_percent > p1->cpu_delay_percent ? 1 : -1);
   case PERCENT_IO_DELAY:
      return (p2->blkio_delay_percent > p1->blkio_delay_percent ? 1 : -1);
   case PERCENT_SWAP_DELAY:
      return (p2->swapin_delay_percent > p1->swapin_delay_percent ? 1 : -1);
   #endif
   case IO_PRIORITY:
      return LinuxProcess_effectiveIOPriority(p1) - LinuxProcess_effectiveIOPriority(p2);
   default:
      return Process_compare(v1, v2);
   }
   test_diff:
   return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
}

bool Process_isThread(Process* this) {
   return (Process_isUserlandThread(this) || Process_isKernelThread(this));
}
