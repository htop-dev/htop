/*
htop - LinuxProcess.c
(C) 2014 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "LinuxProcess.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"


/* semi-global */
long long btime;

/* Used to identify kernel threads in Comm and Exe columns */
static const char *const kthreadID = "KTHREAD";

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
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [M_SHARE] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   [M_TRS] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   [M_DRS] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   [M_LRS] = { .name = "M_LRS", .title = "  LIB ", .description = "The library size of the process (calculated from memory maps)", .flags = PROCESS_FLAG_LINUX_LRS_FIX, },
   [M_DT] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process (unused since Linux 2.6; always 0)", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = "  UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   [CTID] = { .name = "CTID", .title = " CTID    ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   [VPID] = { .name = "VPID", .title = "    VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   [VXID] = { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
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
   [CGROUP] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
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
   [CTXT] = { .name = "CTXT", .title = " CTXT ", .description = "Context switches (incremental sum of voluntary_ctxt_switches and nonvoluntary_ctxt_switches)", .flags = PROCESS_FLAG_LINUX_CTXT, },
   [SECATTR] = { .name = "SECATTR", .title = " Security Attribute ", .description = "Security attribute of the process (e.g. SELinux or AppArmor)", .flags = PROCESS_FLAG_LINUX_SECATTR, },
   [PROC_COMM] = { .name = "COMM", .title = "COMM            ", .description = "comm string of the process from /proc/[pid]/comm", .flags = 0, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process from /proc/[pid]/exe", .flags = 0, },
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

/* This function returns the string displayed in Command column, so that sorting
 * happens on what is displayed - whether comm, full path, basename, etc.. So
 * this follows LinuxProcess_writeField(COMM) and LinuxProcess_writeCommand */
static const char* LinuxProcess_getCommandStr(const Process *this) {
   const LinuxProcess *lp = (const LinuxProcess *)this;
   if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !lp->mergedCommand.str) {
      return this->comm;
   }
   return lp->mergedCommand.str;
}

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
   free(this->secattr);
   free(this->ttyDevice);
   free(this->procExe);
   free(this->procComm);
   free(this->mergedCommand.str);
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

/*
TASK_COMM_LEN is defined to be 16 for /proc/[pid]/comm in man proc(5), but it is
not available in an userspace header - so define it. Note: when colorizing a
basename with the comm prefix, the entire basename (not just the comm prefix) is
colorized for better readability, and it is implicit that only upto
(TASK_COMM_LEN - 1) could be comm
*/
#define TASK_COMM_LEN 16

static bool findCommInCmdline(const char *comm, const char *cmdline, int cmdlineBasenameOffset, int *pCommStart, int *pCommEnd) {
   /* Try to find procComm in tokenized cmdline - this might in rare cases
    * mis-identify a string or fail, if comm or cmdline had been unsuitably
    * modified by the process */
   const char *token;
   const char *tokenBase;
   size_t tokenLen;
   const size_t commLen = strlen(comm);

   for (token = cmdline + cmdlineBasenameOffset; *token; ) {
      for (tokenBase = token; *token && *token != '\n'; ++token) {
         if (*token == '/') {
            tokenBase = token + 1;
         }
      }
      tokenLen = token - tokenBase;

      if ((tokenLen == commLen || (tokenLen > commLen && commLen == (TASK_COMM_LEN - 1))) &&
          strncmp(tokenBase, comm, commLen) == 0) {
         *pCommStart = tokenBase - cmdline;
         *pCommEnd = token - cmdline;
         return true;
      }
      if (*token) {
         do {
            ++token;
         } while ('\n' == *token);
      }
   }
   return false;
}

static int matchCmdlinePrefixWithExeSuffix(const char *cmdline, int cmdlineBaseOffset, const char *exe, int exeBaseOffset, int exeBaseLen) {
   int matchLen;       /* matching length to be returned */
   char delim;         /* delimiter following basename */

   /* cmdline prefix is an absolute path: it must match whole exe. */
   if (cmdline[0] == '/') {
      matchLen = exeBaseLen + exeBaseOffset;
      if (strncmp(cmdline, exe, matchLen) == 0) {
         delim = cmdline[matchLen];
         if (delim == 0 || delim == '\n' || delim == ' ') {
            return matchLen;
         }
      }
      return 0;
   }

   /* cmdline prefix is a relative path: We need to first match the basename at
    * cmdlineBaseOffset and then reverse match the cmdline prefix with the exe
    * suffix. But there is a catch: Some processes modify their cmdline in ways
    * that make htop's identification of the basename in cmdline unreliable.
    * For e.g. /usr/libexec/gdm-session-worker modifies its cmdline to
    * "gdm-session-worker [pam/gdm-autologin]" and htop ends up with
    * procCmdlineBasenameOffset at "gdm-autologin]". This issue could arise with
    * chrome as well as it stores in cmdline its concatenated argument vector,
    * without NUL delimiter between the arguments (which may contain a '/')
    *
    * So if needed, we adjust cmdlineBaseOffset to the previous (if any)
    * component of the cmdline relative path, and retry the procedure. */
   bool delimFound;  /* if valid basename delimiter found */
   do {
      /* match basename */
      matchLen = exeBaseLen + cmdlineBaseOffset;
      if (cmdlineBaseOffset < exeBaseOffset &&
          strncmp(cmdline + cmdlineBaseOffset, exe + exeBaseOffset, exeBaseLen) == 0) {
         delim = cmdline[matchLen];
         if (delim == 0 || delim == '\n' || delim == ' ') {
            int i, j;
            /* reverse match the cmdline prefix and exe suffix */
            for (i = cmdlineBaseOffset - 1, j = exeBaseOffset - 1;
                 i >= 0 && cmdline[i] == exe[j]; --i, --j)
               ;
            /* full match, with exe suffix being a valid relative path */
            if (i < 0 && exe[j] == '/') {
               return matchLen;
            }
         }
      }
      /* Try to find the previous potential cmdlineBaseOffset - it would be
       * preceded by '/' or nothing, and delimited by ' ' or '\n' */
      for (delimFound = false, cmdlineBaseOffset -= 2; cmdlineBaseOffset > 0; --cmdlineBaseOffset) {
         if (delimFound) {
            if (cmdline[cmdlineBaseOffset - 1] == '/') {
               break;
            }
         } else if (cmdline[cmdlineBaseOffset] == ' ' || cmdline[cmdlineBaseOffset] == '\n') {
            delimFound = true;
         }
      }
   } while (delimFound);

   return 0;
}

/* stpcpy, but also converts newlines to spaces */
static inline char *stpcpyWithNewlineConversion(char *dstStr, const char *srcStr) {
   for (; *srcStr; ++srcStr) {
      *dstStr++ = (*srcStr == '\n') ? ' ' : *srcStr;
   }
   *dstStr = 0;
   return dstStr;
}

/*
This function makes the merged Command string. It also stores the offsets of the
basename, comm w.r.t the merged Command string - these offsets will be used by
LinuxProcess_writeCommand() for coloring. The merged Command string is also
returned by LinuxProcess_getCommandStr() for searching, sorting and filtering.
*/
void LinuxProcess_makeCommandStr(Process* this) {
   LinuxProcess *lp = (LinuxProcess *)this;
   LinuxProcessMergedCommand *mc = &lp->mergedCommand;

   bool showMergedCommand = this->settings->showMergedCommand;
   bool showProgramPath = this->settings->showProgramPath;
   bool searchCommInCmdline = this->settings->findCommInCmdline;
   bool stripExeFromCmdline = this->settings->stripExeFromCmdline;

   /* lp->mergedCommand.str needs updating only if its state or contents changed.
    * Its content is based on the fields cmdline, comm, and exe. */
   if (
      mc->prevMergeSet == showMergedCommand &&
      mc->prevPathSet == showProgramPath &&
      mc->prevCommSet == searchCommInCmdline &&
      mc->prevCmdlineSet == stripExeFromCmdline &&
      !mc->cmdlineChanged &&
      !mc->commChanged &&
      !mc->exeChanged
   ) {
      return;
   }

   /* The field separtor "│" has been chosen such that it will not match any
    * valid string used for searching or filtering */
   const char *SEPARATOR = CRT_treeStr[TREE_STR_VERT];
   const int SEPARATOR_LEN = strlen(SEPARATOR);

   /* Check for any changed fields since we last built this string */
   if (mc->cmdlineChanged || mc->commChanged || mc->exeChanged) {
      free(mc->str);
      /* Accommodate the column text, two field separators and terminating NUL */
      mc->str = xCalloc(1, mc->maxLen + 2*SEPARATOR_LEN + 1);
   }

   /* Preserve the settings used in this run */
   mc->prevMergeSet = showMergedCommand;
   mc->prevPathSet = showProgramPath;
   mc->prevCommSet = searchCommInCmdline;
   mc->prevCmdlineSet = stripExeFromCmdline;

   /* Mark everything as unchanged */
   mc->cmdlineChanged = false;
   mc->commChanged = false;
   mc->exeChanged = false;

   /* Clear any separators */
   mc->sep1 = 0;
   mc->sep2 = 0;

   /* Clear any highlighting locations */
   mc->baseStart = 0;
   mc->baseEnd = 0;
   mc->commStart = 0;
   mc->commEnd = 0;

   const char *cmdline = this->comm;
   const char *procExe = lp->procExe;
   const char *procComm = lp->procComm;

   char *strStart = mc->str;
   char *str = strStart;

   int cmdlineBasenameOffset = lp->procCmdlineBasenameOffset;

   if (!showMergedCommand || !procExe || !procComm) {    /* fall back to cmdline */
      if (showMergedCommand && !procExe && procComm && strlen(procComm)) {   /* Prefix column with comm */
         if (strncmp(cmdline + cmdlineBasenameOffset, procComm, MINIMUM(TASK_COMM_LEN - 1, strlen(procComm))) != 0) {
            mc->commStart = 0;
            mc->commEnd = strlen(procComm);

            str = stpcpy(str, procComm);

            mc->sep1 = str - strStart;
            str = stpcpy(str, SEPARATOR);
         }
      }

      if (showProgramPath) {
         (void) stpcpyWithNewlineConversion(str, cmdline);
         mc->baseStart = cmdlineBasenameOffset;
         mc->baseEnd = lp->procCmdlineBasenameEnd;
      } else {
         (void) stpcpyWithNewlineConversion(str, cmdline + cmdlineBasenameOffset);
         mc->baseStart = 0;
         mc->baseEnd = lp->procCmdlineBasenameEnd - cmdlineBasenameOffset;
      }

      if (mc->sep1) {
         mc->baseStart += str - strStart - SEPARATOR_LEN + 1;
         mc->baseEnd += str - strStart - SEPARATOR_LEN + 1;
      }

      return;
   }

   int exeLen = lp->procExeLen;
   int exeBasenameOffset = lp->procExeBasenameOffset;
   int exeBasenameLen = exeLen - exeBasenameOffset;

   /* Start with copying exe */
   if (showProgramPath) {
      str = stpcpy(str, procExe);
      mc->baseStart = exeBasenameOffset;
      mc->baseEnd = exeLen;
   } else {
      str = stpcpy(str, procExe + exeBasenameOffset);
      mc->baseStart = 0;
      mc->baseEnd = exeBasenameLen;
   }

   mc->sep1 = 0;
   mc->sep2 = 0;

   int commStart = 0;
   int commEnd = 0;
   bool commInCmdline = false;

   /* Try to match procComm with procExe's basename: This is reliable (predictable) */
   if (strncmp(procExe + exeBasenameOffset, procComm, TASK_COMM_LEN - 1) == 0) {
      commStart = mc->baseStart;
      commEnd = mc->baseEnd;
   } else if (searchCommInCmdline) {
      /* commStart/commEnd will be adjusted later along with cmdline */
      commInCmdline = findCommInCmdline(procComm, cmdline, cmdlineBasenameOffset, &commStart, &commEnd);
   }

   int matchLen = matchCmdlinePrefixWithExeSuffix(cmdline, cmdlineBasenameOffset, procExe, exeBasenameOffset, exeBasenameLen);

   /* Note: commStart, commEnd are offsets into RichString. But the multibyte
    * separator (with size SEPARATOR_LEN) has size 1 in RichString. The offset
    * adjustments below reflect this. */
   if (commEnd) {
      mc->unmatchedExe = !matchLen;

      if (matchLen) {
         /* strip the matched exe prefix */
         cmdline += matchLen;

         if (commInCmdline) {
            commStart += str - strStart - matchLen;
            commEnd += str - strStart - matchLen;
         }
      } else {
         /* cmdline will be a separate field */
         mc->sep1 = str - strStart;
         str = stpcpy(str, SEPARATOR);

         if (commInCmdline) {
            commStart += str - strStart - SEPARATOR_LEN + 1;
            commEnd += str - strStart - SEPARATOR_LEN + 1;
         }
      }

      mc->separateComm = false;  /* procComm merged */
   } else {
      mc->sep1 = str - strStart;
      str = stpcpy(str, SEPARATOR);

      commStart = str - strStart - SEPARATOR_LEN + 1;
      str = stpcpy(str, procComm);
      commEnd = str - strStart - SEPARATOR_LEN + 1;   /* or commStart + strlen(procComm) */

      mc->unmatchedExe = !matchLen;

      if (matchLen) {
         if (stripExeFromCmdline) {
            cmdline += matchLen;
         }
      }

      if (*cmdline) {
         mc->sep2 = str - strStart - SEPARATOR_LEN + 1;
         str = stpcpy(str, SEPARATOR);
      }

      mc->separateComm = true;  /* procComm a separate field */
   }

   /* Display cmdline if it hasn't been consumed by procExe */
   if (*cmdline) {
      (void) stpcpyWithNewlineConversion(str, cmdline);
   }

   mc->commStart = commStart;
   mc->commEnd = commEnd;
}

static void LinuxProcess_writeCommand(const Process* this, int attr, int baseAttr, RichString* str) {
   const LinuxProcess *lp = (const LinuxProcess *)this;
   const LinuxProcessMergedCommand *mc = &lp->mergedCommand;

   int strStart = RichString_size(str);

   int baseStart = strStart + lp->mergedCommand.baseStart;
   int baseEnd = strStart + lp->mergedCommand.baseEnd;
   int commStart = strStart + lp->mergedCommand.commStart;
   int commEnd = strStart + lp->mergedCommand.commEnd;

   int commAttr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];

   bool highlightBaseName = this->settings->highlightBaseName;

   if(lp->procExeDeleted)
      baseAttr = CRT_colors[FAILED_READ];

   RichString_append(str, attr, lp->mergedCommand.str);

   if (lp->mergedCommand.commEnd) {
      if (lp->mergedCommand.separateComm) {
         RichString_setAttrn(str, commAttr, commStart, commEnd - 1);
      } else if (commStart == baseStart && highlightBaseName) {
         /* If it was matched with procExe's basename, make it bold if needed */
         if (commEnd > baseEnd) {
            RichString_setAttrn(str, A_BOLD | baseAttr, baseStart, baseEnd - 1);
            RichString_setAttrn(str, A_BOLD | commAttr, baseEnd, commEnd - 1);
         } else if (commEnd < baseEnd) {
            RichString_setAttrn(str, A_BOLD | commAttr, commStart, commEnd - 1);
            RichString_setAttrn(str, A_BOLD | baseAttr, commEnd, baseEnd - 1);
         } else {
            // Actually should be highlighted commAttr, but marked baseAttr to reduce visual noise
            RichString_setAttrn(str, A_BOLD | baseAttr, commStart, commEnd - 1);
         }

         baseStart = baseEnd;
      } else {
         RichString_setAttrn(str, commAttr, commStart, commEnd - 1);
      }
   }

   if (baseStart < baseEnd && highlightBaseName) {
      RichString_setAttrn(str, baseAttr, baseStart, baseEnd - 1);
   }

   if (mc->sep1)
      RichString_setAttrn(str, CRT_colors[FAILED_READ], strStart + mc->sep1, strStart + mc->sep1);
   if (mc->sep2)
      RichString_setAttrn(str, CRT_colors[FAILED_READ], strStart + mc->sep2, strStart + mc->sep2);
}

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
      LinuxProcess_writeCommand(this, attr, baseattr, str);
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
      const char* draw = CRT_treeStr[lastItem ? (this->settings->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
      xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
      RichString_append(str, CRT_colors[PROCESS_TREE], buffer);
      LinuxProcess_writeCommand(this, attr, baseattr, str);
   }
}

static void LinuxProcess_writeField(const Process* this, RichString* str, ProcessField field) {
   const LinuxProcess* lp = (const LinuxProcess*) this;
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
   case M_DRS: Process_humanNumber(str, lp->m_drs * CRT_pageSizeKB, coloring); return;
   case M_DT: Process_humanNumber(str, lp->m_dt * CRT_pageSizeKB, coloring); return;
   case M_LRS:
      if (lp->m_lrs) {
         Process_humanNumber(str, lp->m_lrs * CRT_pageSizeKB, coloring);
         return;
      }

      attr = CRT_colors[PROCESS_SHADOW];
      xSnprintf(buffer, n, "  N/A ");
      break;
   case M_TRS: Process_humanNumber(str, lp->m_trs * CRT_pageSizeKB, coloring); return;
   case M_SHARE: Process_humanNumber(str, lp->m_share * CRT_pageSizeKB, coloring); return;
   case M_PSS: Process_humanNumber(str, lp->m_pss, coloring); return;
   case M_SWAP: Process_humanNumber(str, lp->m_swap, coloring); return;
   case M_PSSWP: Process_humanNumber(str, lp->m_psswp, coloring); return;
   case UTIME: Process_printTime(str, lp->utime); return;
   case STIME: Process_printTime(str, lp->stime); return;
   case CUTIME: Process_printTime(str, lp->cutime); return;
   case CSTIME: Process_printTime(str, lp->cstime); return;
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
      double totalRate = NAN;
      if (!isnan(lp->io_rate_read_bps) && !isnan(lp->io_rate_write_bps))
         totalRate = lp->io_rate_read_bps + lp->io_rate_write_bps;
      else if (!isnan(lp->io_rate_read_bps))
         totalRate = lp->io_rate_read_bps;
      else if (!isnan(lp->io_rate_write_bps))
         totalRate = lp->io_rate_write_bps;
      else
         totalRate = NAN;
      Process_outputRate(str, buffer, n, totalRate, coloring); return;
   }
   #ifdef HAVE_OPENVZ
   case CTID: xSnprintf(buffer, n, "%-8s ", lp->ctid ? lp->ctid : ""); break;
   case VPID: xSnprintf(buffer, n, Process_pidFormat, lp->vpid); break;
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
      if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !lp->mergedCommand.str) {
         Process_writeField(this, str, field);
      } else {
         LinuxProcess_writeCommandField(this, str, buffer, n, attr);
      }
      return;
   }
   case PROC_COMM: {
      if (lp->procComm) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];
         /* 15 being (TASK_COMM_LEN - 1) */
         xSnprintf(buffer, n, "%-15.15s ", lp->procComm);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "%-15.15s ", Process_isKernelThread(lp) ? kthreadID : "N/A");
      }
      break;
   }
   case PROC_EXE: {
      if (lp->procExe) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_BASENAME : PROCESS_BASENAME];
         if (lp->procExeDeleted)
            attr = CRT_colors[FAILED_READ];
         xSnprintf(buffer, n, "%-15.15s ", lp->procExe + lp->procExeBasenameOffset);
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "%-15.15s ", Process_isKernelThread(lp) ? kthreadID : "N/A");
      }
      break;
   }
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

static long LinuxProcess_compare(const void* v1, const void* v2) {
   const LinuxProcess *p1, *p2;
   const Settings *settings = ((const Process*)v1)->settings;

   if (settings->direction == 1) {
      p1 = (const LinuxProcess*)v1;
      p2 = (const LinuxProcess*)v2;
   } else {
      p2 = (const LinuxProcess*)v1;
      p1 = (const LinuxProcess*)v2;
   }

   switch ((int)settings->sortKey) {
   case M_DRS:
      return SPACESHIP_NUMBER(p2->m_drs, p1->m_drs);
   case M_DT:
      return SPACESHIP_NUMBER(p2->m_dt, p1->m_dt);
   case M_LRS:
      return SPACESHIP_NUMBER(p2->m_lrs, p1->m_lrs);
   case M_TRS:
      return SPACESHIP_NUMBER(p2->m_trs, p1->m_trs);
   case M_SHARE:
      return SPACESHIP_NUMBER(p2->m_share, p1->m_share);
   case M_PSS:
      return SPACESHIP_NUMBER(p2->m_pss, p1->m_pss);
   case M_SWAP:
      return SPACESHIP_NUMBER(p2->m_swap, p1->m_swap);
   case M_PSSWP:
      return SPACESHIP_NUMBER(p2->m_psswp, p1->m_psswp);
   case UTIME:
      return SPACESHIP_NUMBER(p2->utime, p1->utime);
   case CUTIME:
      return SPACESHIP_NUMBER(p2->cutime, p1->cutime);
   case STIME:
      return SPACESHIP_NUMBER(p2->stime, p1->stime);
   case CSTIME:
      return SPACESHIP_NUMBER(p2->cstime, p1->cstime);
   case RCHAR:
      return SPACESHIP_NUMBER(p2->io_rchar, p1->io_rchar);
   case WCHAR:
      return SPACESHIP_NUMBER(p2->io_wchar, p1->io_wchar);
   case SYSCR:
      return SPACESHIP_NUMBER(p2->io_syscr, p1->io_syscr);
   case SYSCW:
      return SPACESHIP_NUMBER(p2->io_syscw, p1->io_syscw);
   case RBYTES:
      return SPACESHIP_NUMBER(p2->io_read_bytes, p1->io_read_bytes);
   case WBYTES:
      return SPACESHIP_NUMBER(p2->io_write_bytes, p1->io_write_bytes);
   case CNCLWB:
      return SPACESHIP_NUMBER(p2->io_cancelled_write_bytes, p1->io_cancelled_write_bytes);
   case IO_READ_RATE:
      return SPACESHIP_NUMBER(p2->io_rate_read_bps, p1->io_rate_read_bps);
   case IO_WRITE_RATE:
      return SPACESHIP_NUMBER(p2->io_rate_write_bps, p1->io_rate_write_bps);
   case IO_RATE:
      return SPACESHIP_NUMBER(p2->io_rate_read_bps + p2->io_rate_write_bps, p1->io_rate_read_bps + p1->io_rate_write_bps);
   #ifdef HAVE_OPENVZ
   case CTID:
      return SPACESHIP_NULLSTR(p1->ctid, p2->ctid);
   case VPID:
      return SPACESHIP_NUMBER(p2->vpid, p1->vpid);
   #endif
   #ifdef HAVE_VSERVER
   case VXID:
      return SPACESHIP_NUMBER(p2->vxid, p1->vxid);
   #endif
   case CGROUP:
      return SPACESHIP_NULLSTR(p1->cgroup, p2->cgroup);
   case OOM:
      return SPACESHIP_NUMBER(p2->oom, p1->oom);
   #ifdef HAVE_DELAYACCT
   case PERCENT_CPU_DELAY:
      return SPACESHIP_NUMBER(p2->cpu_delay_percent, p1->cpu_delay_percent);
   case PERCENT_IO_DELAY:
      return SPACESHIP_NUMBER(p2->blkio_delay_percent, p1->blkio_delay_percent);
   case PERCENT_SWAP_DELAY:
      return SPACESHIP_NUMBER(p2->swapin_delay_percent, p1->swapin_delay_percent);
   #endif
   case IO_PRIORITY:
      return SPACESHIP_NUMBER(LinuxProcess_effectiveIOPriority(p1), LinuxProcess_effectiveIOPriority(p2));
   case CTXT:
      return SPACESHIP_NUMBER(p2->ctxt_diff, p1->ctxt_diff);
   case SECATTR:
      return SPACESHIP_NULLSTR(p1->secattr, p2->secattr);
   case PROC_COMM: {
      const char *comm1 = p1->procComm ? p1->procComm : (Process_isKernelThread(p1) ? kthreadID : "");
      const char *comm2 = p2->procComm ? p2->procComm : (Process_isKernelThread(p2) ? kthreadID : "");
      return strcmp(comm1, comm2);
   }
   case PROC_EXE: {
      const char *exe1 = p1->procExe ? (p1->procExe + p1->procExeBasenameOffset) : (Process_isKernelThread(p1) ? kthreadID : "");
      const char *exe2 = p2->procExe ? (p2->procExe + p2->procExeBasenameOffset) : (Process_isKernelThread(p2) ? kthreadID : "");
      return strcmp(exe1, exe2);
   }
   default:
      return Process_compare(v1, v2);
   }
}

bool Process_isThread(const Process* this) {
   return (Process_isUserlandThread(this) || Process_isKernelThread(this));
}

const ProcessClass LinuxProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = LinuxProcess_compare
   },
   .writeField = LinuxProcess_writeField,
   .getCommandStr = LinuxProcess_getCommandStr
};
