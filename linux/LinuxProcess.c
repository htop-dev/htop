/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/LinuxProcess.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"
#include "linux/IOPriority.h"


/* semi-global */
int pageSize;
int pageSizeKB;

/* Used to identify kernel threads in Comm and Exe columns */
static const char *const kthreadID = "KTHREAD";

const ProcessFieldData Process_fields[LAST_PROCESSFIELD] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging, I idle)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, .pidColumn = true, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, .pidColumn = true, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, .pidColumn = true, },
   [TTY] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "TPGID", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, .pidColumn = true, },
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
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, .defaultSortDesc = true, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, .defaultSortDesc = true, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, .defaultSortDesc = true, },
   [M_LRS] = { .name = "M_LRS", .title = "  LIB ", .description = "The library size of the process (calculated from memory maps)", .flags = PROCESS_FLAG_LINUX_LRS_FIX, .defaultSortDesc = true, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process (unused since Linux 2.6; always 0)", .flags = 0, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "  UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, .defaultSortDesc = true, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, .defaultSortDesc = true, },
   [TGID] = { .name = "TGID", .title = "TGID", .description = "Thread group ID (i.e. process ID)", .flags = 0, .pidColumn = true, },
#ifdef HAVE_OPENVZ
   [CTID] = { .name = "CTID", .title = " CTID    ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   [VPID] = { .name = "VPID", .title = "VPID", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, .pidColumn = true, },
#endif
#ifdef HAVE_VSERVER
   [VXID] = { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
   [RCHAR] = { .name = "RCHAR", .title = "RCHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [WCHAR] = { .name = "WCHAR", .title = "WCHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [SYSCR] = { .name = "SYSCR", .title = "  READ_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [SYSCW] = { .name = "SYSCW", .title = " WRITE_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [RBYTES] = { .name = "RBYTES", .title = " IO_R ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [WBYTES] = { .name = "WBYTES", .title = " IO_W ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [CNCLWB] = { .name = "CNCLWB", .title = " IO_C ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_READ_RATE] = { .name = "IO_READ_RATE", .title = " DISK READ  ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_WRITE_RATE] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_RATE] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [CGROUP] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
   [OOM] = { .name = "OOM", .title = " OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, .defaultSortDesc = true, },
   [IO_PRIORITY] = { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
#ifdef HAVE_DELAYACCT
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = "IOD% ", .description = "Block I/O delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
#endif
   [M_PSS] = { .name = "M_PSS", .title = "  PSS ", .description = "proportional set size, same as M_RESIDENT but each page is divided by the number of processes sharing it", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_SWAP] = { .name = "M_SWAP", .title = " SWAP ", .description = "Size of the process's swapped pages", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_PSSWP] = { .name = "M_PSSWP", .title = " PSSWP ", .description = "shows proportional swap share of this mapping, unlike \"Swap\", this does not take into account swapped out page of underlying shmem objects", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [CTXT] = { .name = "CTXT", .title = " CTXT ", .description = "Context switches (incremental sum of voluntary_ctxt_switches and nonvoluntary_ctxt_switches)", .flags = PROCESS_FLAG_LINUX_CTXT, .defaultSortDesc = true, },
   [SECATTR] = { .name = "SECATTR", .title = " Security Attribute ", .description = "Security attribute of the process (e.g. SELinux or AppArmor)", .flags = PROCESS_FLAG_LINUX_SECATTR, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process from /proc/[pid]/comm", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process from /proc/[pid]/exe", .flags = 0, },
   [CWD] = { .name ="CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_LINUX_CWD, },
};

Process* LinuxProcess_new(const Settings* settings) {
   LinuxProcess* this = xCalloc(1, sizeof(LinuxProcess));
   Object_setClass(this, Class(LinuxProcess));
   Process_init(&this->super, settings);
   return &this->super;
}

void Process_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
   free(this->cgroup);
#ifdef HAVE_OPENVZ
   free(this->ctid);
#endif
   free(this->cwd);
   free(this->secattr);
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
static int LinuxProcess_effectiveIOPriority(const LinuxProcess* this) {
   if (IOPriority_class(this->ioPriority) == IOPRIO_CLASS_NONE) {
      return IOPriority_tuple(IOPRIO_CLASS_BE, (this->super.nice + 20) / 5);
   }

   return this->ioPriority;
}

#ifdef __ANDROID__
#define SYS_ioprio_get __NR_ioprio_get
#define SYS_ioprio_set __NR_ioprio_set
#endif

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this) {
   IOPriority ioprio = 0;
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_get
   ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->super.pid);
#endif
   this->ioPriority = ioprio;
   return ioprio;
}

bool LinuxProcess_setIOPriority(Process* this, Arg ioprio) {
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_set
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->pid, ioprio.i);
#endif
   return (LinuxProcess_updateIOPriority((LinuxProcess*)this) == ioprio.i);
}

#ifdef HAVE_DELAYACCT
static void LinuxProcess_printDelay(float delay_percent, char* buffer, int n) {
   if (isnan(delay_percent)) {
      xSnprintf(buffer, n, " N/A  ");
   } else {
      xSnprintf(buffer, n, "%4.1f  ", delay_percent);
   }
}
#endif

static void LinuxProcess_writeCommandField(const Process *this, RichString *str, char *buffer, int n, int attr) {
   /* This code is from Process_writeField for COMM, but we invoke
    * LinuxProcess_writeCommand to display
    * /proc/pid/exe (or its basename)│/proc/pid/comm│/proc/pid/cmdline */
   int baseattr = CRT_colors[PROCESS_BASENAME];
   if (this->settings->highlightThreads && Process_isThread(this)) {
      attr = CRT_colors[PROCESS_THREAD];
      baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
   }
   if (!this->settings->treeView || this->indent == 0) {
      Process_writeCommand(this, attr, baseattr, str);
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
      Process_writeCommand(this, attr, baseattr, str);
   }
}

static void LinuxProcess_writeField(const Process* this, RichString* str, ProcessField field) {
   const LinuxProcess* lp = (const LinuxProcess*) this;
   bool coloring = this->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;
   switch (field) {
   case CMINFLT: Process_printCount(str, lp->cminflt, coloring); return;
   case CMAJFLT: Process_printCount(str, lp->cmajflt, coloring); return;
   case M_DRS: Process_printBytes(str, lp->m_drs * pageSize, coloring); return;
   case M_DT: Process_printBytes(str, lp->m_dt * pageSize, coloring); return;
   case M_LRS:
      if (lp->m_lrs) {
         Process_printBytes(str, lp->m_lrs * pageSize, coloring);
         return;
      }

      attr = CRT_colors[PROCESS_SHADOW];
      xSnprintf(buffer, n, "  N/A ");
      break;
   case M_TRS: Process_printBytes(str, lp->m_trs * pageSize, coloring); return;
   case M_SHARE: Process_printBytes(str, lp->m_share * pageSize, coloring); return;
   case M_PSS: Process_printKBytes(str, lp->m_pss, coloring); return;
   case M_SWAP: Process_printKBytes(str, lp->m_swap, coloring); return;
   case M_PSSWP: Process_printKBytes(str, lp->m_psswp, coloring); return;
   case UTIME: Process_printTime(str, lp->utime, coloring); return;
   case STIME: Process_printTime(str, lp->stime, coloring); return;
   case CUTIME: Process_printTime(str, lp->cutime, coloring); return;
   case CSTIME: Process_printTime(str, lp->cstime, coloring); return;
   case RCHAR:  Process_printBytes(str, lp->io_rchar, coloring); return;
   case WCHAR:  Process_printBytes(str, lp->io_wchar, coloring); return;
   case SYSCR:  Process_printCount(str, lp->io_syscr, coloring); return;
   case SYSCW:  Process_printCount(str, lp->io_syscw, coloring); return;
   case RBYTES: Process_printBytes(str, lp->io_read_bytes, coloring); return;
   case WBYTES: Process_printBytes(str, lp->io_write_bytes, coloring); return;
   case CNCLWB: Process_printBytes(str, lp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Process_printRate(str, lp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Process_printRate(str, lp->io_rate_write_bps, coloring); return;
   case IO_RATE: {
      double totalRate;
      if (!isnan(lp->io_rate_read_bps) && !isnan(lp->io_rate_write_bps))
         totalRate = lp->io_rate_read_bps + lp->io_rate_write_bps;
      else if (!isnan(lp->io_rate_read_bps))
         totalRate = lp->io_rate_read_bps;
      else if (!isnan(lp->io_rate_write_bps))
         totalRate = lp->io_rate_write_bps;
      else
         totalRate = NAN;
      Process_printRate(str, totalRate, coloring); return;
   }
   #ifdef HAVE_OPENVZ
   case CTID: xSnprintf(buffer, n, "%-8s ", lp->ctid ? lp->ctid : ""); break;
   case VPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, lp->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: xSnprintf(buffer, n, "%5u ", lp->vxid); break;
   #endif
   case CGROUP: xSnprintf(buffer, n, "%-10s ", lp->cgroup ? lp->cgroup : ""); break;
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
   case CTXT:
      if (lp->ctxt_diff > 1000) {
         attr |= A_BOLD;
      }
      xSnprintf(buffer, n, "%5lu ", lp->ctxt_diff);
      break;
   case SECATTR: snprintf(buffer, n, "%-30s   ", lp->secattr ? lp->secattr : "?"); break;
   case COMM: {
      if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !this->mergedCommand.str) {
         Process_writeField(this, str, field);
      } else {
         LinuxProcess_writeCommandField(this, str, buffer, n, attr);
      }
      return;
   }
   case PROC_COMM: {
      const char* procComm;
      if (this->procComm) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];
         procComm = this->procComm;
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         procComm = Process_isKernelThread(this) ? kthreadID : "N/A";
      }
      /* 15 being (TASK_COMM_LEN - 1) */
      Process_printLeftAlignedField(str, attr, procComm, 15);
      return;
   }
   case PROC_EXE: {
      const char* procExe;
      if (this->procExe) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_BASENAME : PROCESS_BASENAME];
         if (this->procExeDeleted)
            attr = CRT_colors[FAILED_READ];
         procExe = this->procExe + this->procExeBasenameOffset;
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         procExe = Process_isKernelThread(this) ? kthreadID : "N/A";
      }
      Process_printLeftAlignedField(str, attr, procExe, 15);
      return;
   }
   case CWD: {
      const char* cwd;
      if (!lp->cwd) {
         attr = CRT_colors[PROCESS_SHADOW];
         cwd = "N/A";
      } else if (String_startsWith(lp->cwd, "/proc/") && strstr(lp->cwd, " (deleted)") != NULL) {
         attr = CRT_colors[PROCESS_SHADOW];
         cwd = "main thread terminated";
      } else {
         cwd = lp->cwd;
      }
      Process_printLeftAlignedField(str, attr, cwd, 25);
      return;
   }
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_appendAscii(str, attr, buffer);
}

static double adjustNaN(double num) {
   if (isnan(num))
      return -0.0005;

   return num;
}

static int LinuxProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const LinuxProcess* p1 = (const LinuxProcess*)v1;
   const LinuxProcess* p2 = (const LinuxProcess*)v2;

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
   #ifdef HAVE_OPENVZ
   case CTID:
      return SPACESHIP_NULLSTR(p1->ctid, p2->ctid);
   case VPID:
      return SPACESHIP_NUMBER(p1->vpid, p2->vpid);
   #endif
   #ifdef HAVE_VSERVER
   case VXID:
      return SPACESHIP_NUMBER(p1->vxid, p2->vxid);
   #endif
   case CGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup, p2->cgroup);
   case OOM:
      return SPACESHIP_NUMBER(p1->oom, p2->oom);
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY:
      return SPACESHIP_NUMBER(p1->cpu_delay_percent, p2->cpu_delay_percent);
   case PERCENT_IO_DELAY:
      return SPACESHIP_NUMBER(p1->blkio_delay_percent, p2->blkio_delay_percent);
   case PERCENT_SWAP_DELAY:
      return SPACESHIP_NUMBER(p1->swapin_delay_percent, p2->swapin_delay_percent);
   #endif
   case IO_PRIORITY:
      return SPACESHIP_NUMBER(LinuxProcess_effectiveIOPriority(p1), LinuxProcess_effectiveIOPriority(p2));
   case CTXT:
      return SPACESHIP_NUMBER(p1->ctxt_diff, p2->ctxt_diff);
   case SECATTR:
      return SPACESHIP_NULLSTR(p1->secattr, p2->secattr);
   case PROC_COMM: {
      const char *comm1 = v1->procComm ? v1->procComm : (Process_isKernelThread(v1) ? kthreadID : "");
      const char *comm2 = v2->procComm ? v2->procComm : (Process_isKernelThread(v2) ? kthreadID : "");
      return strcmp(comm1, comm2);
   }
   case PROC_EXE: {
      const char *exe1 = v1->procExe ? (v1->procExe + v1->procExeBasenameOffset) : (Process_isKernelThread(v1) ? kthreadID : "");
      const char *exe2 = v2->procExe ? (v2->procExe + v2->procExeBasenameOffset) : (Process_isKernelThread(v2) ? kthreadID : "");
      return strcmp(exe1, exe2);
   }
   case CWD:
      return SPACESHIP_NULLSTR(p1->cwd, p2->cwd);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

const ProcessClass LinuxProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = LinuxProcess_writeField,
   .compareByKey = LinuxProcess_compareByKey
};
