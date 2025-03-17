/*
htop - DarwinProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/DarwinProcess.h"

#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <sys/dirent.h>

#include "CRT.h"
#include "Process.h"
#include "darwin/DarwinMachine.h"
#include "darwin/Platform.h"


const ProcessFieldData Process_fields[LAST_PROCESSFIELD] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line (insert as last column only)", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "PPID", .description = "Parent process ID", .flags = 0, .pidColumn = true, },
   [PGRP] = { .name = "PGRP", .title = "PGRP", .description = "Process group ID", .flags = 0, .pidColumn = true, },
   [SESSION] = { .name = "SESSION", .title = "SID", .description = "Process's session ID", .flags = 0, .pidColumn = true, },
   [TTY] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = PROCESS_FLAG_TTY, },
   [TPGID] = { .name = "TPGID", .title = "TPGID", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, .pidColumn = true, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, .defaultSortDesc = true, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [ELAPSED] = { .name = "ELAPSED", .title = "ELAPSED  ", .description = "Time since the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_VIRT] = { .name = "M_VIRT", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, .defaultSortDesc = true, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, .defaultSortDesc = true, },
   [ST_UID] = { .name = "ST_UID", .title = "UID", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = " CPU%", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, .defaultSortDesc = true, .autoWidth = true, .autoTitleRightAlign = true, },
   [PERCENT_NORM_CPU] = { .name = "PERCENT_NORM_CPU", .title = "NCPU%", .description = "Normalized percentage of the CPU time the process used in the last sampling (normalized by cpu count)", .flags = 0, .defaultSortDesc = true, .autoWidth = true, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, .defaultSortDesc = true, },
   [USER] = { .name = "USER", .title = "USER       ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, .defaultSortDesc = true, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "TGID", .description = "Thread group ID (i.e. process ID)", .flags = 0, .pidColumn = true, },
   [PROC_EXE] = { .name = "EXE", .title = "EXE             ", .description = "Basename of exe of the process from /proc/[pid]/exe", .flags = 0, },
   [CWD] = { .name = "CWD", .title = "CWD                       ", .description = "The current working directory of the process", .flags = PROCESS_FLAG_CWD, },
   [TRANSLATED] = { .name = "TRANSLATED", .title = "T ", .description = "Translation info (T translated, N native)", .flags = 0, },
};

Process* DarwinProcess_new(const Machine* host) {
   DarwinProcess* this = xCalloc(1, sizeof(DarwinProcess));
   Object_setClass(this, Class(DarwinProcess));
   Process_init(&this->super, host);

   this->utime = 0;
   this->stime = 0;
   this->taskAccess = true;
   this->translated = false;
   this->super.state = UNKNOWN;

   return (Process*)this;
}

void Process_delete(Object* cast) {
   DarwinProcess* this = (DarwinProcess*) cast;
   Process_done(&this->super);
   // free platform-specific fields here
   free(this);
}

static void DarwinProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const DarwinProcess* dp = (const DarwinProcess*) super;

   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;

   switch (field) {
   // add Platform-specific fields here
   case TRANSLATED: xSnprintf(buffer, n, "%c ", dp->translated ? 'T' : 'N'); break;
   default:
      Process_writeField(&dp->super, str, field);
      return;
   }

   RichString_appendWide(str, attr, buffer);
}

static int DarwinProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const DarwinProcess* p1 = (const DarwinProcess*)v1;
   const DarwinProcess* p2 = (const DarwinProcess*)v2;

   switch (key) {
   // add Platform-specific fields here
   case TRANSLATED:
      return SPACESHIP_NUMBER(p1->translated, p2->translated);
   default:
      return Process_compareByKey_Base(v1, v2, key);
   }
}

static void DarwinProcess_updateExe(pid_t pid, Process* proc) {
   char path[PROC_PIDPATHINFO_MAXSIZE];

   int r = proc_pidpath(pid, path, sizeof(path));
   if (r <= 0)
      return;

   Process_updateExe(proc, path);
}

static void DarwinProcess_updateCwd(pid_t pid, Process* proc) {
   struct proc_vnodepathinfo vpi;

   int r = proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
   if (r <= 0) {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   if (!vpi.pvi_cdir.vip_path[0]) {
      free(proc->procCwd);
      proc->procCwd = NULL;
      return;
   }

   free_and_xStrdup(&proc->procCwd, vpi.pvi_cdir.vip_path);
}

static void DarwinProcess_updateCmdLine(const struct kinfo_proc* k, Process* proc) {
   Process_updateComm(proc, k->kp_proc.p_comm);

   /* This function is from the old Mac version of htop. Originally from ps? */
   int mib[3], argmax, nargs, c = 0;
   size_t size;
   char *procargs, *sp, *np, *cp;

   /* Get the maximum process arguments size. */
   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;

   size = sizeof( argmax );
   if ( sysctl( mib, 2, &argmax, &size, NULL, 0 ) == -1 ) {
      goto ERROR_A;
   }

   /* Allocate space for the arguments. */
   procargs = (char*)malloc(argmax);
   if ( procargs == NULL ) {
      goto ERROR_A;
   }

   /*
    * Make a sysctl() call to get the raw argument space of the process.
    * The layout is documented in start.s, which is part of the Csu
    * project.  In summary, it looks like:
    *
    * /---------------\ 0x00000000
    * :               :
    * :               :
    * |---------------|
    * | argc          |
    * |---------------|
    * | arg[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | arg[argc - 1] |
    * |---------------|
    * | 0             |
    * |---------------|
    * | env[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | env[n]        |
    * |---------------|
    * | 0             |
    * |---------------| <-- Beginning of data returned by sysctl() is here.
    * | argc          |
    * |---------------|
    * | exec_path     |
    * |:::::::::::::::|
    * |               |
    * | String area.  |
    * |               |
    * |---------------| <-- Top of stack.
    * :               :
    * :               :
    * \---------------/ 0xffffffff
    */
   mib[0] = CTL_KERN;
   mib[1] = KERN_PROCARGS2;
   mib[2] = k->kp_proc.p_pid;

   size = ( size_t ) argmax;
   if ( sysctl( mib, 3, procargs, &size, NULL, 0 ) == -1 ) {
      goto ERROR_B;
   }

   memcpy( &nargs, procargs, sizeof( nargs ) );
   cp = procargs + sizeof( nargs );

   /* Skip the saved exec_path. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         /* End of exec_path reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }

   /* Skip trailing '\0' characters. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp != '\0' ) {
         /* Beginning of first argument reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }
   /* Save where the argv[0] string starts. */
   sp = cp;

   int end = 0;
   for ( np = NULL; c < nargs && cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         c++;
         if ( np != NULL ) {
            /* Convert previous '\0'. */
            *np = ' ';
         }
         /* Note location of current '\0'. */
         np = cp;
         if (end == 0) {
            end = cp - sp;
         }
      }
   }

   /*
    * sp points to the beginning of the arguments/environment string, and
    * np should point to the '\0' terminator for the string.
    */
   if ( np == NULL || np == sp ) {
      /* Empty or unterminated string. */
      goto ERROR_B;
   }
   if (end == 0) {
      end = np - sp;
   }

   Process_updateCmdline(proc, sp, 0, end);

   /* Clean up. */
   free( procargs );

   return;

ERROR_B:
   free( procargs );

ERROR_A:
   Process_updateCmdline(proc, k->kp_proc.p_comm, 0, strlen(k->kp_proc.p_comm));
}

static char* DarwinProcess_getDevname(dev_t dev) {
   if (dev == NODEV) {
      return NULL;
   }
   char buf[sizeof("/dev/") + MAXNAMLEN];
   char* name = devname_r(dev, S_IFCHR, buf, MAXNAMLEN);
   if (name) {
      return xStrdup(name);
   }
   return NULL;
}

void DarwinProcess_setFromKInfoProc(Process* proc, const struct kinfo_proc* ps, bool exists) {
   DarwinProcess* dp = (DarwinProcess*)proc;
   const Settings* settings = proc->super.host->settings;

   const struct extern_proc* ep = &ps->kp_proc;

   /* UNSET HERE :
    *
    * processor
    * user (set at ProcessTable level)
    * nlwp
    * percent_cpu
    * percent_mem
    * m_virt
    * m_resident
    * minflt
    * majflt
    */

   /* First, the "immutable" parts */
   if (!exists) {
      /* Set the PID/PGID/etc. */
      Process_setPid(proc, ep->p_pid);
      Process_setThreadGroup(proc, ep->p_pid);
      Process_setParent(proc, ps->kp_eproc.e_ppid);
      proc->pgrp = ps->kp_eproc.e_pgid;
      proc->session = 0; /* TODO Get the session id */
      proc->tpgid = ps->kp_eproc.e_tpgid;
      proc->isKernelThread = false;
      proc->isUserlandThread = false;
      dp->translated = ps->kp_proc.p_flag & P_TRANSLATED;
      proc->tty_nr = ps->kp_eproc.e_tdev;
      proc->tty_name = NULL;

      proc->starttime_ctime = ep->p_starttime.tv_sec;
      Process_fillStarttimeBuffer(proc);

      DarwinProcess_updateExe(ep->p_pid, proc);
      DarwinProcess_updateCmdLine(ps, proc);

      if (settings->ss->flags & PROCESS_FLAG_CWD) {
         DarwinProcess_updateCwd(ep->p_pid, proc);
      }
   }

   if (proc->tty_name == NULL && (dev_t)proc->tty_nr != NODEV) {
      /* The call to devname() is extremely expensive (due to lstat)
       * and represents ~95% of htop's CPU usage when there is high
       * process turnover.
       *
       * To mitigate this we only fetch TTY information if the TTY
       * field is enabled in the settings.
       */
      if (settings->ss->flags & PROCESS_FLAG_TTY) {
         proc->tty_name = DarwinProcess_getDevname(proc->tty_nr);
         if (!proc->tty_name) {
            /* devname failed: prevent us from calling it again */
            proc->tty_nr = NODEV;
         }
      }
   }

   /* Mutable information */
   proc->nice = ep->p_nice;
   proc->priority = ep->p_priority;

   proc->state = (ep->p_stat == SZOMB) ? ZOMBIE : UNKNOWN;

   /* Make sure the updated flag is set */
   proc->super.updated = true;
}

void DarwinProcess_setFromLibprocPidinfo(DarwinProcess* proc, DarwinProcessTable* dpt, double timeIntervalNS) {
   struct proc_taskinfo pti;

   if (PROC_PIDTASKINFO_SIZE != proc_pidinfo(Process_getPid(&proc->super), PROC_PIDTASKINFO, 0, &pti, PROC_PIDTASKINFO_SIZE)) {
      proc->taskAccess = false;
      return;
   }

   const DarwinMachine* dhost = (const DarwinMachine*) proc->super.super.host;

   uint64_t total_existing_time_ns = proc->stime + proc->utime;
   uint64_t user_time_ns = pti.pti_total_user;
   uint64_t system_time_ns = pti.pti_total_system;
   uint64_t total_current_time_ns = user_time_ns + system_time_ns;

   if (total_existing_time_ns < total_current_time_ns) {
      const uint64_t total_time_diff_ns = total_current_time_ns - total_existing_time_ns;
      proc->super.percent_cpu = ((double)total_time_diff_ns / timeIntervalNS) * 100.0;
   } else {
      proc->super.percent_cpu = 0.0;
   }
   Process_updateCPUFieldWidths(proc->super.percent_cpu);

   proc->super.state = pti.pti_numrunning > 0 ? RUNNING : SLEEPING;
   // Convert from nanoseconds to hundredths of seconds
   proc->super.time = total_current_time_ns / 10000000ULL;
   proc->super.nlwp = pti.pti_threadnum;
   proc->super.m_virt = pti.pti_virtual_size / ONE_K;
   proc->super.m_resident = pti.pti_resident_size / ONE_K;
   proc->super.majflt = pti.pti_faults;
   proc->super.percent_mem = (double)pti.pti_resident_size * 100.0 / (double)dhost->host_info.max_mem;

   proc->stime = system_time_ns;
   proc->utime = user_time_ns;

   dpt->super.kernelThreads += 0; /*pti.pti_threads_system;*/
   dpt->super.userlandThreads += pti.pti_threadnum; /*pti.pti_threads_user;*/
   dpt->super.totalTasks += pti.pti_threadnum;
   dpt->super.runningTasks += pti.pti_numrunning;
}

/*
 * Scan threads for process state information.
 * Based on: http://stackoverflow.com/questions/6788274/ios-mac-cpu-usage-for-thread
 * and       https://github.com/max-horvath/htop-osx/blob/e86692e869e30b0bc7264b3675d2a4014866ef46/ProcessList.c
 */
void DarwinProcess_scanThreads(DarwinProcess* dp, DarwinProcessTable* dpt) {
   Process* proc = (Process*) dp;
   kern_return_t ret;

   if (!dp->taskAccess) {
      return;
   }

   if (proc->state == ZOMBIE) {
      return;
   }

   pid_t pid = Process_getPid(proc);

   task_t task;
   ret = task_for_pid(mach_task_self(), pid, &task);
   if (ret != KERN_SUCCESS) {
      // TODO: workaround for modern MacOS limits on task_for_pid()
      if (ret != KERN_FAILURE)
         CRT_debug("task_for_pid(%d) failed: %s", pid, mach_error_string(ret));
      dp->taskAccess = false;
      return;
   }

   thread_array_t thread_list;
   mach_msg_type_number_t thread_count;
   ret = task_threads(task, &thread_list, &thread_count);
   if (ret != KERN_SUCCESS) {
      CRT_debug("task_threads(%d) failed: %s", pid, mach_error_string(ret));
      dp->taskAccess = false;
      mach_port_deallocate(mach_task_self(), task);
      return;
   }

   const bool hideUserlandThreads = dpt->super.super.host->settings->hideUserlandThreads;
   bool isProcessStuck = false;

   for (mach_msg_type_number_t i = 0; i < thread_count; i++) {

      thread_identifier_info_data_t identifer_info;
      mach_msg_type_number_t identifer_info_count = THREAD_IDENTIFIER_INFO_COUNT;
      ret = thread_info(thread_list[i], THREAD_IDENTIFIER_INFO, (thread_info_t) &identifer_info, &identifer_info_count);
      if (ret != KERN_SUCCESS) {
         CRT_debug("thread_info(%d:%d) for identifier failed: %s", pid, i, mach_error_string(ret));
         continue;
      }

      uint64_t tid = identifer_info.thread_id;

      bool preExisting;
      Process *tprocess = ProcessTable_getProcess(&dpt->super, tid, &preExisting, DarwinProcess_new);
      tprocess->super.updated = true;
      dpt->super.totalTasks++;

      if (hideUserlandThreads) {
         tprocess->super.show = false;
         continue;
      }

      pid_t tprocessPid = Process_getPid(tprocess);
      assert(tprocessPid >= 0);
      assert((uint64_t)tprocessPid == tid);
      (void)tprocessPid;

      Process_setParent(tprocess, pid);
      Process_setThreadGroup(tprocess, pid);
      tprocess->super.show       = true;
      tprocess->isUserlandThread = true;
      tprocess->st_uid           = proc->st_uid;
      tprocess->user             = proc->user;

      thread_extended_info_data_t extended_info;
      mach_msg_type_number_t extended_info_count = THREAD_EXTENDED_INFO_COUNT;
      ret = thread_info(thread_list[i], THREAD_EXTENDED_INFO, (thread_info_t) &extended_info, &extended_info_count);
      if (ret != KERN_SUCCESS) {
         CRT_debug("thread_info(%d:%d) for extended failed: %s", pid, i, mach_error_string(ret));
         continue;
      }

      DarwinProcess* tdproc     = (DarwinProcess*)tprocess;
      tdproc->super.percent_cpu = extended_info.pth_cpu_usage / 10.0;
      tdproc->stime             = extended_info.pth_system_time;
      tdproc->utime             = extended_info.pth_user_time;
      tdproc->super.time        = (extended_info.pth_system_time + extended_info.pth_user_time) / 10000000;
      tdproc->super.priority    = extended_info.pth_curpri;

      if (extended_info.pth_run_state == TH_STATE_UNINTERRUPTIBLE) {
         isProcessStuck |= true;
         tdproc->super.state = UNINTERRUPTIBLE_WAIT;
      }

      // TODO: depend on setting
      const char* name = extended_info.pth_name[0] != '\0' ? extended_info.pth_name : proc->procComm;
      Process_updateCmdline(tprocess, name, 0, strlen(name));

      if (!preExisting)
         ProcessTable_add(&dpt->super, tprocess);
   }

   if (isProcessStuck) {
      dp->super.state = UNINTERRUPTIBLE_WAIT;
   }

   vm_deallocate(mach_task_self(), (vm_address_t) thread_list, sizeof(thread_port_array_t) * thread_count);
   mach_port_deallocate(mach_task_self(), task);
}


const ProcessClass DarwinProcess_class = {
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
      .writeField = DarwinProcess_rowWriteField
   },
   .compareByKey = DarwinProcess_compareByKey
};
