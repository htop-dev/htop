/*
htop - PCPProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPProcessList.h"

#include <math.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "Settings.h"
#include "XUtils.h"

#include "pcp/PCPProcess.h"


static int PCPProcessList_computeCPUcount(void) {
   int cpus;
   if ((cpus = Platform_getMaxCPU()) <= 0)
      cpus = Metric_instanceCount(PCP_PERCPU_SYSTEM);
   return cpus > 1 ? cpus : 1;
}

static void PCPProcessList_updateCPUcount(PCPProcessList* this) {
   ProcessList* pl = &(this->super);
   unsigned int cpus = PCPProcessList_computeCPUcount();
   if (cpus == pl->cpuCount)
      return;

   pl->cpuCount = cpus;
   free(this->percpu);
   free(this->values);

   this->percpu = xCalloc(cpus, sizeof(pmAtomValue *));
   for (unsigned int i = 0; i < cpus; i++)
      this->percpu[i] = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   this->values = xCalloc(cpus, sizeof(pmAtomValue));
}

static char* setUser(UsersTable* this, unsigned int uid, int pid, int offset) {
   char* name = Hashtable_get(this->users, uid);
   if (name)
      return name;

   pmAtomValue value;
   if (Metric_instance(PCP_PROC_ID_USER, pid, offset, &value, PM_TYPE_STRING)) {
      Hashtable_put(this->users, uid, value.cp);
      name = value.cp;
   }
   return name;
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   PCPProcessList* this = xCalloc(1, sizeof(PCPProcessList));
   ProcessList* super = &(this->super);

   ProcessList_init(super, Class(PCPProcess), usersTable, pidMatchList, userId);

   struct timeval timestamp;
   gettimeofday(&timestamp, NULL);
   this->timestamp = pmtimevalToReal(&timestamp);

   this->cpu = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   PCPProcessList_updateCPUcount(this);

   return super;
}

void ProcessList_delete(ProcessList* pl) {
   PCPProcessList* this = (PCPProcessList*) pl;
   ProcessList_done(pl);
   free(this->values);
   for (unsigned int i = 0; i < pl->cpuCount; i++)
      free(this->percpu[i]);
   free(this->percpu);
   free(this->cpu);
   free(this);
}

static inline unsigned long Metric_instance_s32(int metric, int pid, int offset, unsigned long fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_32))
      return value.l;
   return fallback;
}

static inline unsigned long Metric_instance_u32(int metric, int pid, int offset, unsigned long fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_U32))
      return value.ul;
   return fallback;
}

static inline unsigned long long Metric_instance_u64(int metric, int pid, int offset, unsigned long long fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_U64))
      return value.ull;
   return fallback;
}

static inline unsigned long long Metric_instance_time(int metric, int pid, int offset) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_U64))
      return value.ull / 10;
   return 0;
}

static inline unsigned long long Metric_instance_ONE_K(int metric, int pid, int offset) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_U64))
      return value.ull / ONE_K;
   return ULLONG_MAX;
}

static inline char Metric_instance_char(int metric, int pid, int offset, char fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_STRING)) {
      char uchar = value.cp[0];
      free(value.cp);
      return uchar;
   }
   return fallback;
}

static void PCPProcessList_updateID(Process* process, int pid, int offset) {
   process->tgid = Metric_instance_u32(PCP_PROC_TGID, pid, offset, 1);
   process->ppid = Metric_instance_u32(PCP_PROC_PPID, pid, offset, 1);
   process->state = Metric_instance_char(PCP_PROC_STATE, pid, offset, '?');
}

static void PCPProcessList_updateInfo(Process* process, int pid, int offset, char* command, size_t commLen) {
   PCPProcess* pp = (PCPProcess*) process;
   pmAtomValue value;

   if (!Metric_instance(PCP_PROC_CMD, pid, offset, &value, PM_TYPE_STRING))
      value.cp = xStrdup("<unknown>");
   String_safeStrncpy(command, value.cp, commLen);
   free(value.cp);

   process->pgrp = Metric_instance_u32(PCP_PROC_PGRP, pid, offset, 0);
   process->session = Metric_instance_u32(PCP_PROC_SESSION, pid, offset, 0);
   process->tty_nr = Metric_instance_u32(PCP_PROC_TTY, pid, offset, 0);
   process->tpgid = Metric_instance_u32(PCP_PROC_TTYPGRP, pid, offset, 0);
   process->minflt = Metric_instance_u32(PCP_PROC_MINFLT, pid, offset, 0);
   pp->cminflt = Metric_instance_u32(PCP_PROC_CMINFLT, pid, offset, 0);
   process->majflt = Metric_instance_u32(PCP_PROC_MAJFLT, pid, offset, 0);
   pp->cmajflt = Metric_instance_u32(PCP_PROC_CMAJFLT, pid, offset, 0);
   pp->utime = Metric_instance_u64(PCP_PROC_UTIME, pid, offset, 0);
   pp->stime = Metric_instance_u64(PCP_PROC_STIME, pid, offset, 0);
   pp->cutime = Metric_instance_time(PCP_PROC_CUTIME, pid, offset);
   pp->cstime = Metric_instance_time(PCP_PROC_CSTIME, pid, offset);
   process->priority = Metric_instance_u32(PCP_PROC_PRIORITY, pid, offset, 0);
   process->nice = Metric_instance_s32(PCP_PROC_NICE, pid, offset, 0);
   process->nlwp = Metric_instance_u32(PCP_PROC_THREADS, pid, offset, 0);
   process->starttime_ctime = Metric_instance_time(PCP_PROC_STARTTIME, pid, offset);
   process->processor = Metric_instance_u32(PCP_PROC_PROCESSOR, pid, offset, 0);

   process->time = pp->utime + pp->stime;
}

static void PCPProcessList_updateIO(PCPProcess* pp, int pid, int offset, unsigned long long now) {
   pmAtomValue value;

   pp->io_rchar = Metric_instance_ONE_K(PCP_PROC_IO_RCHAR, pid, offset);
   pp->io_wchar = Metric_instance_ONE_K(PCP_PROC_IO_WCHAR, pid, offset);
   pp->io_syscr = Metric_instance_u64(PCP_PROC_IO_SYSCR, pid, offset, ULLONG_MAX);
   pp->io_syscw = Metric_instance_u64(PCP_PROC_IO_SYSCW, pid, offset, ULLONG_MAX);
   pp->io_cancelled_write_bytes = Metric_instance_ONE_K(PCP_PROC_IO_CANCELLED, pid, offset);

   if (Metric_instance(PCP_PROC_IO_READB, pid, offset, &value, PM_TYPE_U64)) {
      unsigned long long last_read = pp->io_read_bytes;
      pp->io_read_bytes = value.ull / ONE_K;
      pp->io_rate_read_bps = ONE_K * (pp->io_read_bytes - last_read) /
                                     (now - pp->io_last_scan_time);
   } else {
      pp->io_read_bytes = ULLONG_MAX;
      pp->io_rate_read_bps = NAN;
   }

   if (Metric_instance(PCP_PROC_IO_WRITEB, pid, offset, &value, PM_TYPE_U64)) {
      unsigned long long last_write = pp->io_write_bytes;
      pp->io_write_bytes = value.ull;
      pp->io_rate_write_bps = ONE_K * (pp->io_write_bytes - last_write) /
                                      (now - pp->io_last_scan_time);
   } else {
      pp->io_write_bytes = ULLONG_MAX;
      pp->io_rate_write_bps = NAN;
   }

   pp->io_last_scan_time = now;
}

static void PCPProcessList_updateMemory(PCPProcess* pp, int pid, int offset) {
   pp->super.m_virt = Metric_instance_u32(PCP_PROC_MEM_SIZE, pid, offset, 0);
   pp->super.m_resident = Metric_instance_u32(PCP_PROC_MEM_RSS, pid, offset, 0);
   pp->m_share = Metric_instance_u32(PCP_PROC_MEM_SHARE, pid, offset, 0);
   pp->m_trs = Metric_instance_u32(PCP_PROC_MEM_TEXTRS, pid, offset, 0);
   pp->m_lrs = Metric_instance_u32(PCP_PROC_MEM_LIBRS, pid, offset, 0);
   pp->m_drs = Metric_instance_u32(PCP_PROC_MEM_DATRS, pid, offset, 0);
   pp->m_dt = Metric_instance_u32(PCP_PROC_MEM_DIRTY, pid, offset, 0);
}

static void PCPProcessList_updateSmaps(PCPProcess* pp, pid_t pid, int offset) {
   pp->m_pss = Metric_instance_u64(PCP_PROC_SMAPS_PSS, pid, offset, 0);
   pp->m_swap = Metric_instance_u64(PCP_PROC_SMAPS_SWAP, pid, offset, 0);
   pp->m_psswp = Metric_instance_u64(PCP_PROC_SMAPS_SWAPPSS, pid, offset, 0);
}

static void PCPProcessList_readOomData(PCPProcess* pp, int pid, int offset) {
   pp->oom = Metric_instance_u32(PCP_PROC_OOMSCORE, pid, offset, 0);
}

static void PCPProcessList_readCtxtData(PCPProcess* pp, int pid, int offset) {
   pmAtomValue value;
   unsigned long ctxt = 0;

   if (Metric_instance(PCP_PROC_VCTXSW, pid, offset, &value, PM_TYPE_U32))
      ctxt += value.ul;
   if (Metric_instance(PCP_PROC_NVCTXSW, pid, offset, &value, PM_TYPE_U32))
      ctxt += value.ul;

   pp->ctxt_diff = ctxt > pp->ctxt_total ? ctxt - pp->ctxt_total : 0;
   pp->ctxt_total = ctxt;
}

static char* setString(Metric metric, int pid, int offset, char* string) {
   if (string)
      free(string);
   pmAtomValue value;
   if (Metric_instance(metric, pid, offset, &value, PM_TYPE_STRING))
      string = value.cp;
   else
      string = NULL;
   return string;
}

static void PCPProcessList_updateTTY(Process* process, int pid, int offset) {
   process->tty_name = setString(PCP_PROC_TTYNAME, pid, offset, process->tty_name);
}

static void PCPProcessList_readCGroups(PCPProcess* pp, int pid, int offset) {
   pp->cgroup = setString(PCP_PROC_CGROUPS, pid, offset, pp->cgroup);
}

static void PCPProcessList_readSecattrData(PCPProcess* pp, int pid, int offset) {
   pp->secattr = setString(PCP_PROC_LABELS, pid, offset, pp->secattr);
}

static void PCPProcessList_readCwd(PCPProcess* pp, int pid, int offset) {
   pp->super.procCwd = setString(PCP_PROC_CWD, pid, offset, pp->super.procCwd);
}

static void PCPProcessList_updateUsername(Process* process, int pid, int offset, UsersTable* users) {
   process->st_uid = Metric_instance_u32(PCP_PROC_ID_UID, pid, offset, 0);
   process->user = setUser(users, process->st_uid, pid, offset);
}

static void PCPProcessList_updateCmdline(Process* process, int pid, int offset, const char* comm) {
   pmAtomValue value;
   if (!Metric_instance(PCP_PROC_PSARGS, pid, offset, &value, PM_TYPE_STRING)) {
      if (process->state != 'Z')
         process->isKernelThread = true;
      Process_updateCmdline(process, NULL, 0, 0);
      return;
   }

   char *command = value.cp;
   int length = strlen(command);
   if (command[0] != '(') {
      process->isKernelThread = false;
   } else {
      ++command;
      --length;
      if (command[length-1] == ')')
         command[length-1] = '\0';
      process->isKernelThread = true;
   }

   int tokenStart = 0;
   for (int i = 0; i < length; i++) {
      /* htop considers the next character after the last / that is before
       * basenameOffset, as the start of the basename in cmdline - see
       * Process_writeCommand */
      if (command[i] == '/')
         tokenStart = i + 1;
   }
   int tokenEnd = length;

   Process_updateCmdline(process, command, tokenStart, tokenEnd);
   free(value.cp);

   Process_updateComm(process, comm);

   if (Metric_instance(PCP_PROC_EXE, pid, offset, &value, PM_TYPE_STRING)) {
      Process_updateExe(process, value.cp);
      free(value.cp);
   }
}

static bool PCPProcessList_updateProcesses(PCPProcessList* this, double period, struct timeval* tv) {
   ProcessList* pl = (ProcessList*) this;
   const Settings* settings = pl->settings;

   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;

   unsigned long long now = tv->tv_sec * 1000LL + tv->tv_usec / 1000LL;
   int pid = -1, offset = -1;

   /* for every process ... */
   while (Metric_iterate(PCP_PROC_PID, &pid, &offset)) {

      bool preExisting;
      Process* proc = ProcessList_getProcess(pl, pid, &preExisting, PCPProcess_new);
      PCPProcess* pp = (PCPProcess*) proc;
      PCPProcessList_updateID(proc, pid, offset);

      /*
       * These conditions will not trigger on first occurrence, cause we need to
       * add the process to the ProcessList and do all one time scans
       * (e.g. parsing the cmdline to detect a kernel thread)
       * But it will short-circuit subsequent scans.
       */
      if (preExisting && hideKernelThreads && Process_isKernelThread(proc)) {
         proc->updated = true;
         proc->show = false;
         if (proc->state == 'R')
            pl->runningTasks++;
         pl->kernelThreads++;
         pl->totalTasks++;
         continue;
      }
      if (preExisting && hideUserlandThreads && Process_isUserlandThread(proc)) {
         proc->updated = true;
         proc->show = false;
         if (proc->state == 'R')
            pl->runningTasks++;
         pl->userlandThreads++;
         pl->totalTasks++;
         continue;
      }

      if (settings->flags & PROCESS_FLAG_IO)
         PCPProcessList_updateIO(pp, pid, offset, now);

      PCPProcessList_updateMemory(pp, pid, offset);

      if ((settings->flags & PROCESS_FLAG_LINUX_SMAPS) &&
          (Process_isKernelThread(proc) == false)) {
         if (Metric_enabled(PCP_PROC_SMAPS_PSS))
            PCPProcessList_updateSmaps(pp, pid, offset);
      }

      char command[MAX_NAME + 1];
      unsigned int tty_nr = proc->tty_nr;
      unsigned long long int lasttimes = pp->utime + pp->stime;

      PCPProcessList_updateInfo(proc, pid, offset, command, sizeof(command));
      proc->starttime_ctime += Platform_getBootTime();
      if (tty_nr != proc->tty_nr)
         PCPProcessList_updateTTY(proc, pid, offset);

      float percent_cpu = (pp->utime + pp->stime - lasttimes) / period * 100.0;
      proc->percent_cpu = isnan(percent_cpu) ?
                          0.0 : CLAMP(percent_cpu, 0.0, pl->cpuCount * 100.0);
      proc->percent_mem = proc->m_resident / (double)pl->totalMem * 100.0;

      if (!preExisting) {
         PCPProcessList_updateUsername(proc, pid, offset, pl->usersTable);
         PCPProcessList_updateCmdline(proc, pid, offset, command);
         Process_fillStarttimeBuffer(proc);
         ProcessList_add(pl, proc);
      } else if (settings->updateProcessNames && proc->state != 'Z') {
         PCPProcessList_updateCmdline(proc, pid, offset, command);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_CGROUP)
         PCPProcessList_readCGroups(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_OOM)
         PCPProcessList_readOomData(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_CTXT)
         PCPProcessList_readCtxtData(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_LINUX_SECATTR)
         PCPProcessList_readSecattrData(pp, pid, offset);

      if (settings->flags & PROCESS_FLAG_CWD)
         PCPProcessList_readCwd(pp, pid, offset);

      if (proc->state == 'Z' && !proc->cmdline && command[0]) {
         Process_updateCmdline(proc, command, 0, strlen(command));
      } else if (Process_isThread(proc)) {
         if ((settings->showThreadNames || Process_isKernelThread(proc)) && command[0]) {
            Process_updateCmdline(proc, command, 0, strlen(command));
         }

         if (Process_isKernelThread(proc)) {
            pl->kernelThreads++;
         } else {
            pl->userlandThreads++;
         }
      }

      /* Set at the end when we know if a new entry is a thread */
      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) ||
                      (hideUserlandThreads && Process_isUserlandThread(proc)));

      pl->totalTasks++;
      if (proc->state == 'R')
         pl->runningTasks++;
      proc->updated = true;
   }
   return true;
}

static void PCPProcessList_updateMemoryInfo(ProcessList* super) {
   unsigned long long int freeMem = 0;
   unsigned long long int swapFreeMem = 0;
   unsigned long long int sreclaimableMem = 0;
   super->totalMem = super->usedMem = super->cachedMem = 0;
   super->usedSwap = super->totalSwap = 0;

   pmAtomValue value;
   if (Metric_values(PCP_MEM_TOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalMem = value.ull;
   if (Metric_values(PCP_MEM_FREE, &value, 1, PM_TYPE_U64) != NULL)
      freeMem = value.ull;
   if (Metric_values(PCP_MEM_BUFFERS, &value, 1, PM_TYPE_U64) != NULL)
      super->buffersMem = value.ull;
   if (Metric_values(PCP_MEM_SRECLAIM, &value, 1, PM_TYPE_U64) != NULL)
      sreclaimableMem = value.ull;
   if (Metric_values(PCP_MEM_SHARED, &value, 1, PM_TYPE_U64) != NULL)
      super->sharedMem = value.ull;
   if (Metric_values(PCP_MEM_CACHED, &value, 1, PM_TYPE_U64) != NULL) {
      super->cachedMem = value.ull;
      super->cachedMem += sreclaimableMem;
   }
   const memory_t usedDiff = freeMem + super->cachedMem + sreclaimableMem + super->buffersMem + super->sharedMem;
   super->usedMem = (super->totalMem >= usedDiff) ?
           super->totalMem - usedDiff : super->totalMem - freeMem;
   if (Metric_values(PCP_MEM_AVAILABLE, &value, 1, PM_TYPE_U64) != NULL)
      super->availableMem = MINIMUM(value.ull, super->totalMem);
   else
      super->availableMem = freeMem;
   if (Metric_values(PCP_MEM_SWAPFREE, &value, 1, PM_TYPE_U64) != NULL)
      swapFreeMem = value.ull;
   if (Metric_values(PCP_MEM_SWAPTOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalSwap = value.ull;
   if (Metric_values(PCP_MEM_SWAPCACHED, &value, 1, PM_TYPE_U64) != NULL)
      super->cachedSwap = value.ull;
    super->usedSwap = super->totalSwap - swapFreeMem - super->cachedSwap;
}

/* make copies of previously sampled values to avoid overwrite */
static inline void PCPProcessList_backupCPUTime(pmAtomValue* values) {
   /* the PERIOD fields (must) mirror the TIME fields */
   for (int metric = CPU_TOTAL_TIME; metric < CPU_TOTAL_PERIOD; metric++) {
      values[metric + CPU_TOTAL_PERIOD] = values[metric];
   }
}

static inline void PCPProcessList_saveCPUTimePeriod(pmAtomValue* values, CPUMetric previous, pmAtomValue* latest) {
   pmAtomValue *value;

   /* new value for period */
   value = &values[previous];
   if (latest->ull > value->ull)
      value->ull = latest->ull - value->ull;
   else
      value->ull = 0;

   /* new value for time */
   value = &values[previous - CPU_TOTAL_PERIOD];
   value->ull = latest->ull;
}

/* using copied sampled values and new values, calculate derivations */
static void PCPProcessList_deriveCPUTime(pmAtomValue* values) {

   pmAtomValue* usertime = &values[CPU_USER_TIME];
   pmAtomValue* guesttime = &values[CPU_GUEST_TIME];
   usertime->ull -= guesttime->ull;

   pmAtomValue* nicetime = &values[CPU_NICE_TIME];
   pmAtomValue* guestnicetime = &values[CPU_GUESTNICE_TIME];
   nicetime->ull -= guestnicetime->ull;

   pmAtomValue* idletime = &values[CPU_IDLE_TIME];
   pmAtomValue* iowaittime = &values[CPU_IOWAIT_TIME];
   pmAtomValue* idlealltime = &values[CPU_IDLE_ALL_TIME];
   idlealltime->ull = idletime->ull + iowaittime->ull;

   pmAtomValue* systemtime = &values[CPU_SYSTEM_TIME];
   pmAtomValue* irqtime = &values[CPU_IRQ_TIME];
   pmAtomValue* softirqtime = &values[CPU_SOFTIRQ_TIME];
   pmAtomValue* systalltime = &values[CPU_SYSTEM_ALL_TIME];
   systalltime->ull = systemtime->ull + irqtime->ull + softirqtime->ull;

   pmAtomValue* virtalltime = &values[CPU_GUEST_TIME];
   virtalltime->ull = guesttime->ull + guestnicetime->ull;

   pmAtomValue* stealtime = &values[CPU_STEAL_TIME];
   pmAtomValue* totaltime = &values[CPU_TOTAL_TIME];
   totaltime->ull = usertime->ull + nicetime->ull + systalltime->ull +
                    idlealltime->ull + stealtime->ull + virtalltime->ull;

   PCPProcessList_saveCPUTimePeriod(values, CPU_USER_PERIOD, usertime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_NICE_PERIOD, nicetime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SYSTEM_PERIOD, systemtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SYSTEM_ALL_PERIOD, systalltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IDLE_ALL_PERIOD, idlealltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IDLE_PERIOD, idletime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IOWAIT_PERIOD, iowaittime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_IRQ_PERIOD, irqtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_SOFTIRQ_PERIOD, softirqtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_STEAL_PERIOD, stealtime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_GUEST_PERIOD, virtalltime);
   PCPProcessList_saveCPUTimePeriod(values, CPU_TOTAL_PERIOD, totaltime);
}

static void PCPProcessList_updateAllCPUTime(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   pmAtomValue* value = &this->cpu[cpumetric];
   if (Metric_values(metric, value, 1, PM_TYPE_U64) == NULL)
      memset(&value, 0, sizeof(pmAtomValue));
}

static void PCPProcessList_updatePerCPUTime(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.cpuCount;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_U64) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].ull = this->values[i].ull;
}

static void PCPProcessList_updatePerCPUReal(PCPProcessList* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.cpuCount;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_DOUBLE) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].d = this->values[i].d;
}

static void PCPProcessList_updateHeader(ProcessList* super, const Settings* settings) {
   PCPProcessList_updateMemoryInfo(super);

   PCPProcessList* this = (PCPProcessList*) super;
   PCPProcessList_updateCPUcount(this);

   PCPProcessList_backupCPUTime(this->cpu);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_USER, CPU_USER_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_NICE, CPU_NICE_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IDLE, CPU_IDLE_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_IRQ, CPU_IRQ_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_STEAL, CPU_STEAL_TIME);
   PCPProcessList_updateAllCPUTime(this, PCP_CPU_GUEST, CPU_GUEST_TIME);
   PCPProcessList_deriveCPUTime(this->cpu);

   for (unsigned int i = 0; i < super->cpuCount; i++)
      PCPProcessList_backupCPUTime(this->percpu[i]);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_USER, CPU_USER_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_NICE, CPU_NICE_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IDLE, CPU_IDLE_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_IRQ, CPU_IRQ_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_STEAL, CPU_STEAL_TIME);
   PCPProcessList_updatePerCPUTime(this, PCP_PERCPU_GUEST, CPU_GUEST_TIME);
   for (unsigned int i = 0; i < super->cpuCount; i++)
      PCPProcessList_deriveCPUTime(this->percpu[i]);

   if (settings->showCPUFrequency)
      PCPProcessList_updatePerCPUReal(this, PCP_HINV_CPUCLOCK, CPU_FREQUENCY);
}

static inline void PCPProcessList_scanZfsArcstats(PCPProcessList* this) {
   unsigned long long int dbufSize = 0;
   unsigned long long int dnodeSize = 0;
   unsigned long long int bonusSize = 0;
   pmAtomValue value;

   memset(&this->zfs, 0, sizeof(ZfsArcStats));
   if (Metric_values(PCP_ZFS_ARC_ANON_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.anon = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_C_MAX, &value, 1, PM_TYPE_U64))
      this->zfs.max = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_BONUS_SIZE, &value, 1, PM_TYPE_U64))
      bonusSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_DBUF_SIZE, &value, 1, PM_TYPE_U64))
      dbufSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_DNODE_SIZE, &value, 1, PM_TYPE_U64))
      dnodeSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_COMPRESSED_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.compressed = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_UNCOMPRESSED_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.uncompressed = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_HDR_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.header = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_MFU_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.MFU = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_MRU_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.MRU = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.size = value.ull / ONE_K;

   this->zfs.other = (dbufSize + dnodeSize + bonusSize) / ONE_K;
   this->zfs.enabled = (this->zfs.size > 0);
   this->zfs.isCompressed = (this->zfs.compressed > 0);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   PCPProcessList* this = (PCPProcessList*) super;
   const Settings* settings = super->settings;
   bool enabled = !pauseProcessUpdate;

   bool flagged = settings->showCPUFrequency;
   Metric_enable(PCP_HINV_CPUCLOCK, flagged);

   /* In pause mode do not sample per-process metric values at all */
   for (int metric = PCP_PROC_PID; metric < PCP_METRIC_COUNT; metric++)
      Metric_enable(metric, enabled);

   flagged = settings->flags & PROCESS_FLAG_LINUX_CGROUP;
   Metric_enable(PCP_PROC_CGROUPS, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_OOM;
   Metric_enable(PCP_PROC_OOMSCORE, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_CTXT;
   Metric_enable(PCP_PROC_VCTXSW, flagged && enabled);
   Metric_enable(PCP_PROC_NVCTXSW, flagged && enabled);
   flagged = settings->flags & PROCESS_FLAG_LINUX_SECATTR;
   Metric_enable(PCP_PROC_LABELS, flagged && enabled);

   /* Sample smaps metrics on every second pass to improve performance */
   static int smaps_flag;
   smaps_flag = !!smaps_flag;
   Metric_enable(PCP_PROC_SMAPS_PSS, smaps_flag && enabled);
   Metric_enable(PCP_PROC_SMAPS_SWAP, smaps_flag && enabled);
   Metric_enable(PCP_PROC_SMAPS_SWAPPSS, smaps_flag && enabled);

   struct timeval timestamp;
   Metric_fetch(&timestamp);

   double sample = this->timestamp;
   this->timestamp = pmtimevalToReal(&timestamp);

   PCPProcessList_updateHeader(super, settings);
   PCPProcessList_scanZfsArcstats(this);

   /* In pause mode only update global data for meters (CPU, memory, etc) */
   if (pauseProcessUpdate)
      return;

   double period = (this->timestamp - sample) * 100;
   PCPProcessList_updateProcesses(this, period, &timestamp);
}
