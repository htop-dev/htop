/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LinuxProcess.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "RowField.h"
#include "Scheduling.h"
#include "Settings.h"
#include "XUtils.h"
#include "linux/IOPriority.h"
#include "linux/LinuxMachine.h"


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
   [ELAPSED] = { .name = "ELAPSED", .title = "ELAPSED  ", .description = "Time since the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, .defaultSortDesc = true, },
   [M_PRIV] = { .name = "M_PRIV", .title = " PRIV ", .description = "The private memory size of the process - resident set size minus shared memory", .flags = 0, .defaultSortDesc = true, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the .text segment of the process (CODE)", .flags = 0, .defaultSortDesc = true, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the .data segment plus stack usage of the process (DATA)", .flags = 0, .defaultSortDesc = true, },
   [M_LRS] = { .name = "M_LRS", .title = "  LIB ", .description = "The library size of the process (calculated from memory maps)", .flags = PROCESS_FLAG_LINUX_LRS_FIX, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "UID", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = " CPU%", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER       ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
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
   [IO_READ_RATE] = { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_WRITE_RATE] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [IO_RATE] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, .defaultSortDesc = true, },
   [CGROUP] = { .name = "CGROUP", .title = "CGROUP (raw)", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, .autoWidth = true, },
   [CCGROUP] = { .name = "CCGROUP", .title = "CGROUP (compressed)", .description = "Which cgroup the process is in (condensed to essentials)", .flags = PROCESS_FLAG_LINUX_CGROUP, .autoWidth = true, },
   [CONTAINER] = { .name = "CONTAINER", .title = "CONTAINER", .description = "Name of the container the process is in (guessed by heuristics)", .flags = PROCESS_FLAG_LINUX_CGROUP, .autoWidth = true, },
   [OOM] = { .name = "OOM", .title = " OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, .defaultSortDesc = true, },
   [IO_PRIORITY] = { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
#ifdef HAVE_DELAYACCT
   [PERCENT_CPU_DELAY] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
   [PERCENT_IO_DELAY] = { .name = "PERCENT_IO_DELAY", .title = " IOD% ", .description = "Block I/O delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
   [PERCENT_SWAP_DELAY] = { .name = "PERCENT_SWAP_DELAY", .title = "SWPD% ", .description = "Swapin delay %", .flags = PROCESS_FLAG_LINUX_DELAYACCT, .defaultSortDesc = true, },
#endif
   [M_PSS] = { .name = "M_PSS", .title = "  PSS ", .description = "proportional set size, same as M_RESIDENT but each page is divided by the number of processes sharing it", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_SWAP] = { .name = "M_SWAP", .title = " SWAP ", .description = "Size of the process's swapped pages", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [M_PSSWP] = { .name = "M_PSSWP", .title = " PSSWP ", .description = "shows proportional swap share of this mapping, unlike \"Swap\", this does not take into account swapped out page of underlying shmem objects", .flags = PROCESS_FLAG_LINUX_SMAPS, .defaultSortDesc = true, },
   [CTXT] = { .name = "CTXT", .title = " CTXT ", .description = "Context switches (incremental sum of voluntary_ctxt_switches and nonvoluntary_ctxt_switches)", .flags = PROCESS_FLAG_LINUX_CTXT, .defaultSortDesc = true, },
   [SECATTR] = { .name = "SECATTR", .title = "Security Attribute", .description = "Security attribute of the process (e.g. SELinux or AppArmor)", .flags = PROCESS_FLAG_LINUX_SECATTR, .autoWidth = true, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process from /proc/[pid]/comm", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process from /proc/[pid]/exe", .flags = 0, },
   [CWD] = { .name = "CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_CWD, },
   [AUTOGROUP_ID] = { .name = "AUTOGROUP_ID", .title = "AGRP", .description = "The autogroup identifier of the process", .flags = PROCESS_FLAG_LINUX_AUTOGROUP, },
   [AUTOGROUP_NICE] = { .name = "AUTOGROUP_NICE", .title = " ANI", .description = "Nice value (the higher the value, the more other processes take priority) associated with the process autogroup", .flags = PROCESS_FLAG_LINUX_AUTOGROUP, },
#ifdef SCHEDULER_SUPPORT
   [SCHEDULERPOLICY] = { .name = "SCHEDULERPOLICY", .title = "SCHED ", .description = "Current scheduling policy of the process", .flags = PROCESS_FLAG_SCHEDPOL, },
#endif
};

Process* LinuxProcess_new(const Machine* host) {
   LinuxProcess* this = xCalloc(1, sizeof(LinuxProcess));
   Object_setClass(this, Class(LinuxProcess));
   Process_init(&this->super, host);
   return &this->super;
}

void Process_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
   free(this->container_short);
   free(this->cgroup_short);
   free(this->cgroup);
#ifdef HAVE_OPENVZ
   free(this->ctid);
#endif
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

IOPriority LinuxProcess_updateIOPriority(Process* p) {
   IOPriority ioprio = 0;
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_get
   ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, Process_getPid(p));
#endif
   LinuxProcess* this = (LinuxProcess*) p;
   this->ioPriority = ioprio;
   return ioprio;
}

static bool LinuxProcess_setIOPriority(Process* p, Arg ioprio) {
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_set
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, Process_getPid(p), ioprio.i);
#endif
   return LinuxProcess_updateIOPriority(p) == ioprio.i;
}

bool LinuxProcess_rowSetIOPriority(Row* super, Arg ioprio) {
   Process* p = (Process*) super;
   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   return LinuxProcess_setIOPriority(p, ioprio);
}

bool LinuxProcess_isAutogroupEnabled(void) {
   char buf[16];
   if (xReadfile(PROCDIR "/sys/kernel/sched_autogroup_enabled", buf, sizeof(buf)) < 0)
      return false;
   return buf[0] == '1';
}

static bool LinuxProcess_changeAutogroupPriorityBy(Process* p, Arg delta) {
   char buffer[256];
   pid_t pid = Process_getPid(p);
   xSnprintf(buffer, sizeof(buffer), PROCDIR "/%d/autogroup", pid);

   FILE* file = fopen(buffer, "r+");
   if (!file)
      return false;

   long int identity;
   int nice;
   int ok = fscanf(file, "/autogroup-%ld nice %d", &identity, &nice);
   bool success = false;
   if (ok == 2 && fseek(file, 0L, SEEK_SET) == 0) {
      xSnprintf(buffer, sizeof(buffer), "%d", nice + delta.i);
      success = fputs(buffer, file) > 0;
   }

   fclose(file);
   return success;
}

bool LinuxProcess_rowChangeAutogroupPriorityBy(Row* super, Arg delta) {
   Process* p = (Process*) super;
   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   return LinuxProcess_changeAutogroupPriorityBy(p, delta);
}

static double LinuxProcess_totalIORate(const LinuxProcess* lp) {
   double totalRate = NAN;
   if (isNonnegative(lp->io_rate_read_bps)) {
      totalRate = lp->io_rate_read_bps;
      if (isNonnegative(lp->io_rate_write_bps)) {
         totalRate += lp->io_rate_write_bps;
      }
   } else if (isNonnegative(lp->io_rate_write_bps)) {
      totalRate = lp->io_rate_write_bps;
   }
   return totalRate;
}

static void LinuxProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const Process* this = (const Process*) super;
   const LinuxProcess* lp = (const LinuxProcess*) super;
   const Machine* host = (const Machine*) super->host;
   const LinuxMachine* lhost = (const LinuxMachine*) super->host;

   bool coloring = host->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;

   switch (field) {
   case CMINFLT: Row_printCount(str, lp->cminflt, coloring); return;
   case CMAJFLT: Row_printCount(str, lp->cmajflt, coloring); return;
   case M_DRS: Row_printBytes(str, lp->m_drs * lhost->pageSize, coloring); return;
   case M_LRS:
      if (lp->m_lrs) {
         Row_printBytes(str, lp->m_lrs * lhost->pageSize, coloring);
         return;
      }

      attr = CRT_colors[PROCESS_SHADOW];
      xSnprintf(buffer, n, "  N/A ");
      break;
   case M_TRS: Row_printBytes(str, lp->m_trs * lhost->pageSize, coloring); return;
   case M_SHARE: Row_printBytes(str, lp->m_share * lhost->pageSize, coloring); return;
   case M_PRIV: Row_printKBytes(str, lp->m_priv, coloring); return;
   case M_PSS: Row_printKBytes(str, lp->m_pss, coloring); return;
   case M_SWAP: Row_printKBytes(str, lp->m_swap, coloring); return;
   case M_PSSWP: Row_printKBytes(str, lp->m_psswp, coloring); return;
   case UTIME: Row_printTime(str, lp->utime, coloring); return;
   case STIME: Row_printTime(str, lp->stime, coloring); return;
   case CUTIME: Row_printTime(str, lp->cutime, coloring); return;
   case CSTIME: Row_printTime(str, lp->cstime, coloring); return;
   case RCHAR:  Row_printBytes(str, lp->io_rchar, coloring); return;
   case WCHAR:  Row_printBytes(str, lp->io_wchar, coloring); return;
   case SYSCR:  Row_printCount(str, lp->io_syscr, coloring); return;
   case SYSCW:  Row_printCount(str, lp->io_syscw, coloring); return;
   case RBYTES: Row_printBytes(str, lp->io_read_bytes, coloring); return;
   case WBYTES: Row_printBytes(str, lp->io_write_bytes, coloring); return;
   case CNCLWB: Row_printBytes(str, lp->io_cancelled_write_bytes, coloring); return;
   case IO_READ_RATE:  Row_printRate(str, lp->io_rate_read_bps, coloring); return;
   case IO_WRITE_RATE: Row_printRate(str, lp->io_rate_write_bps, coloring); return;
   case IO_RATE: Row_printRate(str, LinuxProcess_totalIORate(lp), coloring); return;
   #ifdef HAVE_OPENVZ
   case CTID: xSnprintf(buffer, n, "%-8s ", lp->ctid ? lp->ctid : ""); break;
   case VPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, lp->vpid); break;
   #endif
   #ifdef HAVE_VSERVER
   case VXID: xSnprintf(buffer, n, "%5u ", lp->vxid); break;
   #endif
   case CGROUP: xSnprintf(buffer, n, "%-*.*s ", Row_fieldWidths[CGROUP], Row_fieldWidths[CGROUP], lp->cgroup ? lp->cgroup : "N/A"); break;
   case CCGROUP: xSnprintf(buffer, n, "%-*.*s ", Row_fieldWidths[CCGROUP], Row_fieldWidths[CCGROUP], lp->cgroup_short ? lp->cgroup_short : (lp->cgroup ? lp->cgroup : "N/A")); break;
   case CONTAINER: xSnprintf(buffer, n, "%-*.*s ", Row_fieldWidths[CONTAINER], Row_fieldWidths[CONTAINER], lp->container_short ? lp->container_short : "N/A"); break;
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
   case PERCENT_CPU_DELAY: Row_printPercentage(lp->cpu_delay_percent, buffer, n, 5, &attr); break;
   case PERCENT_IO_DELAY: Row_printPercentage(lp->blkio_delay_percent, buffer, n, 5, &attr); break;
   case PERCENT_SWAP_DELAY: Row_printPercentage(lp->swapin_delay_percent, buffer, n, 5, &attr); break;
   #endif
   case CTXT:
      if (lp->ctxt_diff > 1000) {
         attr |= A_BOLD;
      }
      xSnprintf(buffer, n, "%5lu ", lp->ctxt_diff);
      break;
   case SECATTR: snprintf(buffer, n, "%-*.*s ", Row_fieldWidths[SECATTR], Row_fieldWidths[SECATTR], lp->secattr ? lp->secattr : "N/A"); break;
   case AUTOGROUP_ID:
      if (lp->autogroup_id != -1) {
         xSnprintf(buffer, n, "%4ld ", lp->autogroup_id);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, " N/A ");
      }
      break;
   case AUTOGROUP_NICE:
      if (lp->autogroup_id != -1) {
         xSnprintf(buffer, n, "%3d ", lp->autogroup_nice);
         attr = lp->autogroup_nice < 0 ? CRT_colors[PROCESS_HIGH_PRIORITY]
            : lp->autogroup_nice > 0 ? CRT_colors[PROCESS_LOW_PRIORITY]
            : CRT_colors[PROCESS_SHADOW];
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "N/A ");
      }
      break;
   default:
      Process_writeField(this, str, field);
      return;
   }

   RichString_appendAscii(str, attr, buffer);
}

static int LinuxProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const LinuxProcess* p1 = (const LinuxProcess*)v1;
   const LinuxProcess* p2 = (const LinuxProcess*)v2;

   switch (key) {
   case M_DRS:
      return SPACESHIP_NUMBER(p1->m_drs, p2->m_drs);
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
      return compareRealNumbers(LinuxProcess_totalIORate(p1), LinuxProcess_totalIORate(p2));
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
   case CCGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup_short, p2->cgroup_short);
   case CONTAINER:
      return SPACESHIP_NULLSTR(p1->container_short, p2->container_short);
   case OOM:
      return SPACESHIP_NUMBER(p1->oom, p2->oom);
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY:
      return compareRealNumbers(p1->cpu_delay_percent, p2->cpu_delay_percent);
   case PERCENT_IO_DELAY:
      return compareRealNumbers(p1->blkio_delay_percent, p2->blkio_delay_percent);
   case PERCENT_SWAP_DELAY:
      return compareRealNumbers(p1->swapin_delay_percent, p2->swapin_delay_percent);
   #endif
   case IO_PRIORITY:
      return SPACESHIP_NUMBER(LinuxProcess_effectiveIOPriority(p1), LinuxProcess_effectiveIOPriority(p2));
   case CTXT:
      return SPACESHIP_NUMBER(p1->ctxt_diff, p2->ctxt_diff);
   case SECATTR:
      return SPACESHIP_NULLSTR(p1->secattr, p2->secattr);
   case AUTOGROUP_ID:
      return SPACESHIP_NUMBER(p1->autogroup_id, p2->autogroup_id);
   case AUTOGROUP_NICE:
      return SPACESHIP_NUMBER(p1->autogroup_nice, p2->autogroup_nice);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

const ProcessClass LinuxProcess_class = {
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
      .writeField = LinuxProcess_rowWriteField
   },
   .compareByKey = LinuxProcess_compareByKey
};
