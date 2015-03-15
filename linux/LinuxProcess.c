/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
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
#include <sys/syscall.h>

/*{

#define PROCESS_FLAG_LINUX_IOPRIO   0x0100
#define PROCESS_FLAG_LINUX_OPENVZ   0x0200
#define PROCESS_FLAG_LINUX_VSERVER  0x0400
#define PROCESS_FLAG_LINUX_CGROUP   0x0800

typedef enum LinuxProcessFields {
   PID = 1, COMM, STATE, PPID, PGRP, SESSION, TTY_NR, TPGID, FLAGS, MINFLT, CMINFLT, MAJFLT, CMAJFLT, UTIME,
   STIME, CUTIME, CSTIME, PRIORITY, NICE, ITREALVALUE, STARTTIME, VSIZE, RSS, RLIM, STARTCODE, ENDCODE,
   STARTSTACK, KSTKESP, KSTKEIP, SIGNAL, BLOCKED, SSIGIGNORE, SIGCATCH, WCHAN, NSWAP, CNSWAP, EXIT_SIGNAL,
   PROCESSOR, M_SIZE, M_RESIDENT, M_SHARE, M_TRS, M_DRS, M_LRS, M_DT, ST_UID, PERCENT_CPU, PERCENT_MEM,
   USER, TIME, NLWP, TGID,
   #ifdef HAVE_OPENVZ
   CTID, VPID,
   #endif
   #ifdef HAVE_VSERVER
   VXID,
   #endif
   #ifdef HAVE_TASKSTATS
   RCHAR, WCHAR, SYSCR, SYSCW, RBYTES, WBYTES, CNCLWB, IO_READ_RATE, IO_WRITE_RATE, IO_RATE,
   #endif
   #ifdef HAVE_CGROUP
   CGROUP,
   #endif
   #ifdef HAVE_OOM
   OOM,
   #endif
   IO_PRIORITY,
   LAST_PROCESSFIELD
} LinuxProcessField;

#include "IOPriority.h"

typedef struct LinuxProcess_ {
   Process super;
   IOPriority ioPriority;
} LinuxProcess;

#define Process_delete LinuxProcess_delete

}*/

ProcessFieldData Process_fields[] = {
   { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   { .name = "SESSION", .title = "   SESN ", .description = "Process's session ID", .flags = 0, },
   { .name = "TTY_NR", .title = "  TTY ", .description = "Controlling terminal", .flags = 0, },
   { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   { .name = "FLAGS", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
   { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, },
   { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, },
   { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, },
   { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, },
   { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   { .name = "ITREALVALUE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   { .name = "VSIZE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "RSS", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "RLIM", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTCODE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "ENDCODE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "STARTSTACK", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "KSTKESP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "KSTKEIP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "BLOCKED", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGIGNORE", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "SIGCATCH", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "WCHAN", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "NSWAP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "CNSWAP", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "EXIT_SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   { .name = "M_LRS", .title = " LIB ", .description = "The library size of the process", .flags = 0, },
   { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process", .flags = 0, },
   { .name = "ST_UID", .title = " UID ", .description = "User ID of the process owner", .flags = 0, },
   { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   { .name = "CTID", .title = "   CTID ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   { .name = "VPID", .title = " VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
#ifdef HAVE_TASKSTATS
   { .name = "RCHAR", .title = "    RD_CHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, },
   { .name = "WCHAR", .title = "    WR_CHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, },
   { .name = "SYSCR", .title = "    RD_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "SYSCW", .title = "    WR_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "RBYTES", .title = "  IO_RBYTES ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "WBYTES", .title = "  IO_WBYTES ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "CNCLWB", .title = "  IO_CANCEL ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, },
#endif
#ifdef HAVE_CGROUP
   { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
#endif
#ifdef HAVE_OOM
   { .name = "OOM", .title = "    OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = 0, },
#endif
   { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
   { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

char* Process_pidFormat = "%7u ";
char* Process_tpgidFormat = "%7u ";

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1) return;
   if (maxPid > 99999) {
      Process_fields[PID].title =     "    PID ";
      Process_fields[PPID].title =    "   PPID ";
      #ifdef HAVE_OPENVZ
      Process_fields[VPID].title =    "   VPID ";
      #endif
      Process_fields[TPGID].title =   "  TPGID ";
      Process_fields[TGID].title =    "   TGID ";
      Process_fields[PGRP].title =    "   PGRP ";
      Process_fields[SESSION].title = "   SESN ";
      #ifdef HAVE_OOM
      Process_fields[OOM].title =     "    OOM ";
      #endif
      Process_pidFormat = "%7u ";
      Process_tpgidFormat = "%7d ";
   } else {
      Process_fields[PID].title =     "  PID ";
      Process_fields[PPID].title =    " PPID ";
      #ifdef HAVE_OPENVZ
      Process_fields[VPID].title =    " VPID ";
      #endif
      Process_fields[TPGID].title =   "TPGID ";
      Process_fields[TGID].title =    " TGID ";
      Process_fields[PGRP].title =    " PGRP ";
      Process_fields[SESSION].title = " SESN ";
      #ifdef HAVE_OOM
      Process_fields[OOM].title =     "  OOM ";
      #endif
      Process_pidFormat = "%5u ";
      Process_tpgidFormat = "%5d ";
   }
}

LinuxProcess* LinuxProcess_new(Settings* settings) {
   LinuxProcess* this = calloc(sizeof(LinuxProcess), 1);
   Object_setClass(this, Class(Process));
   Process_init(&this->super, settings);
   return this;
}

void LinuxProcess_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
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
   IOPriority ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->super.pid);
   this->ioPriority = ioprio;
   return ioprio;
}

bool LinuxProcess_setIOPriority(LinuxProcess* this, IOPriority ioprio) {
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->super.pid, ioprio);
   return (LinuxProcess_updateIOPriority(this) == ioprio);
}

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   LinuxProcess* lp = (LinuxProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   int n = sizeof(buffer) - 1;
   switch (field) {
   case IO_PRIORITY: {
      int klass = IOPriority_class(lp->ioPriority);
      if (klass == IOPRIO_CLASS_NONE) {
         // see note [1] above
         snprintf(buffer, n, "B%1d ", (int) (this->nice + 20) / 5);
      } else if (klass == IOPRIO_CLASS_BE) {
         snprintf(buffer, n, "B%1d ", IOPriority_data(lp->ioPriority));
      } else if (klass == IOPRIO_CLASS_RT) {
         attr = CRT_colors[PROCESS_HIGH_PRIORITY];
         snprintf(buffer, n, "R%1d ", IOPriority_data(lp->ioPriority));
      } else if (lp->ioPriority == IOPriority_Idle) {
         attr = CRT_colors[PROCESS_LOW_PRIORITY]; 
         snprintf(buffer, n, "id ");
      } else {
         snprintf(buffer, n, "?? ");
      }
      break;
   }
   default:
      Process_writeDefaultField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long Process_compare(const void* v1, const void* v2) {
   LinuxProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (LinuxProcess*)v1;
      p2 = (LinuxProcess*)v2;
   } else {
      p2 = (LinuxProcess*)v1;
      p1 = (LinuxProcess*)v2;
   }
   switch (settings->sortKey) {
   case IO_PRIORITY:
      return LinuxProcess_effectiveIOPriority(p1) - LinuxProcess_effectiveIOPriority(p2);
   default:
      return Process_defaultCompare(v1, v2);
   }
}
