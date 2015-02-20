/*
htop - LinuxProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LinuxProcessList.h"
#include "LinuxProcess.h"
#include "CRT.h"
#include "String.h"
#include <errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>

/*{

#include "ProcessList.h"

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

}*/
   
ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* this = calloc(1, sizeof(ProcessList));
   ProcessList_init(this, usersTable, pidWhiteList);

   // Update CPU count:
   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   char buffer[256];
   int cpus = -1;
   do {
      cpus++;
      fgets(buffer, 255, file);
   } while (String_startsWith(buffer, "cpu"));
   fclose(file);

   this->cpuCount = MAX(cpus - 1, 1);
   this->cpus = calloc(cpus, sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   #ifdef HAVE_OPENVZ
   this->flags |= PROCESS_FLAG_OPENVZ;
   #endif

   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static ssize_t xread(int fd, void *buf, size_t count) {
  // Read some bytes. Retry on EINTR and when we don't get as many bytes as we requested.
  size_t alreadyRead = 0;
  for(;;) {
     ssize_t res = read(fd, buf, count);
     if (res == -1 && errno == EINTR) continue;
     if (res > 0) {
       buf = ((char*)buf)+res;
       count -= res;
       alreadyRead += res;
     }
     if (res == -1) return -1;
     if (count == 0 || res == 0) return alreadyRead;
  }
}

static bool LinuxProcessList_readStatFile(Process *process, const char* dirname, const char* name, char* command) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   static char buf[MAX_READ+1];

   int size = xread(fd, buf, MAX_READ);
   close(fd);
   if (size <= 0) return false;
   buf[size] = '\0';

   assert(process->pid == atoi(buf));
   char *location = strchr(buf, ' ');
   if (!location) return false;

   location += 2;
   char *end = strrchr(location, ')');
   if (!end) return false;
   
   int commsize = end - location;
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   location = end + 2;

   process->state = location[0];
   location += 2;
   process->ppid = strtol(location, &location, 10);
   location += 1;
   process->pgrp = strtoul(location, &location, 10);
   location += 1;
   process->session = strtoul(location, &location, 10);
   location += 1;
   process->tty_nr = strtoul(location, &location, 10);
   location += 1;
   process->tpgid = strtol(location, &location, 10);
   location += 1;
   process->flags = strtoul(location, &location, 10);
   location += 1;
   process->minflt = strtoull(location, &location, 10);
   location += 1;
   process->cminflt = strtoull(location, &location, 10);
   location += 1;
   process->majflt = strtoull(location, &location, 10);
   location += 1;
   process->cmajflt = strtoull(location, &location, 10);
   location += 1;
   process->utime = strtoull(location, &location, 10);
   location += 1;
   process->stime = strtoull(location, &location, 10);
   location += 1;
   process->cutime = strtoull(location, &location, 10);
   location += 1;
   process->cstime = strtoull(location, &location, 10);
   location += 1;
   process->priority = strtol(location, &location, 10);
   location += 1;
   process->nice = strtol(location, &location, 10);
   location += 1;
   process->nlwp = strtol(location, &location, 10);
   location += 1;
   for (int i=0; i<17; i++) location = strchr(location, ' ')+1;
   process->exit_signal = strtol(location, &location, 10);
   location += 1;
   assert(location != NULL);
   process->processor = strtol(location, &location, 10);

   return true;
}


static bool LinuxProcessList_statProcessDir(Process* process, const char* dirname, char* name, time_t curTime) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   snprintf(filename, MAX_NAME, "%s/%s", dirname, name);
   struct stat sstat;
   int statok = stat(filename, &sstat);
   if (statok == -1)
      return false;
   process->st_uid = sstat.st_uid;
  
   struct tm date;
   time_t ctime = sstat.st_ctime;
   process->starttime_ctime = ctime;
   (void) localtime_r((time_t*) &ctime, &date);
   strftime(process->starttime_show, 7, ((ctime > curTime - 86400) ? "%R " : "%b%d "), &date);
   
   return true;
}

#ifdef HAVE_TASKSTATS

static void LinuxProcessList_readIoFile(Process* process, const char* dirname, char* name, unsigned long long now) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   snprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return;
   
   char buffer[1024];
   ssize_t buflen = xread(fd, buffer, 1023);
   close(fd);
   if (buflen < 1) return;
   buffer[buflen] = '\0';
   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   char *buf = buffer;
   char *line = NULL;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
      case 'r':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_rchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "ead_bytes: ", 11) == 0) {
            process->io_read_bytes = strtoull(line+12, NULL, 10);
            process->io_rate_read_bps = 
               ((double)(process->io_read_bytes - last_read))/(((double)(now - process->io_rate_read_time))/1000);
            process->io_rate_read_time = now;
         }
         break;
      case 'w':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_wchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "rite_bytes: ", 12) == 0) {
            process->io_write_bytes = strtoull(line+13, NULL, 10);
            process->io_rate_write_bps = 
               ((double)(process->io_write_bytes - last_write))/(((double)(now - process->io_rate_write_time))/1000);
            process->io_rate_write_time = now;
         }
         break;
      case 's':
         if (line[5] == 'r' && strncmp(line+1, "yscr: ", 6) == 0)
            process->io_syscr = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "yscw: ", 6) == 0)
            sscanf(line, "syscw: %32llu", &process->io_syscw);
            process->io_syscw = strtoull(line+7, NULL, 10);
         break;
      case 'c':
         if (strncmp(line+1, "ancelled_write_bytes: ", 22) == 0)
           process->io_cancelled_write_bytes = strtoull(line+23, NULL, 10);
      }
   }
}

#endif



static bool LinuxProcessList_readStatmFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/statm", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
   char buf[256];
   ssize_t rres = xread(fd, buf, 255);
   close(fd);
   if (rres < 1) return false;

   char *p = buf;
   errno = 0;
   process->m_size = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_resident = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_share = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_trs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_lrs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_drs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_dt = strtol(p, &p, 10);
   return (errno == 0);
}

#ifdef HAVE_OPENVZ

static void LinuxProcessList_readOpenVZData(ProcessList* this, Process* process, const char* dirname, const char* name) {
   if ( (!(this->flags & PROCESS_FLAG_OPENVZ)) || (access("/proc/vz", R_OK) != 0)) {
      process->vpid = process->pid;
      process->ctid = 0;
      this->flags |= ~PROCESS_FLAG_OPENVZ;
      return;
   }
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) 
      return;
   fscanf(file, 
      "%*32u %*32s %*1c %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %32u %32u",
      &process->vpid, &process->ctid);
   fclose(file);
}

#endif

#ifdef HAVE_CGROUP

static void LinuxProcessList_readCGroupFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/cgroup", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      process->cgroup = strdup("");
      return;
   }
   char buffer[256];
   char *ok = fgets(buffer, 255, file);
   if (ok) {
      char* trimmed = String_trim(buffer);
      int nFields;
      char** fields = String_split(trimmed, ':', &nFields);
      free(trimmed);
      free(process->cgroup);
      if (nFields >= 3) {
         process->cgroup = strndup(fields[2] + 1, 10);
      } else {
         process->cgroup = strdup("");
      }
      String_freeArray(fields);
   }
   fclose(file);
}

#endif

#ifdef HAVE_VSERVER

static void LinuxProcessList_readVServerData(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   char buffer[256];
   process->vxid = 0;
   while (fgets(buffer, 255, file)) {
      if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

#ifdef HAVE_OOM

static void LinuxProcessList_readOomData(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/oom_score", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   char buffer[256];
   if (fgets(buffer, 255, file)) {
      unsigned int oom;
      int ok = sscanf(buffer, "%32u", &oom);
      if (ok >= 1) {
         process->oom = oom;
      }
   }
   fclose(file);
}

#endif

static bool LinuxProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   if (Process_isKernelThread(process))
      return true;

   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
         
   char command[4096+1]; // max cmdline length on Linux
   int amtRead = xread(fd, command, sizeof(command) - 1);
   close(fd);
   int tokenEnd = 0; 
   if (amtRead > 0) {
      for (int i = 0; i < amtRead; i++)
         if (command[i] == '\0' || command[i] == '\n') {
            if (tokenEnd == 0) {
               tokenEnd = i;
            }
            command[i] = ' ';
         }
   }
   if (tokenEnd == 0) {
      tokenEnd = amtRead;
   }
   command[amtRead] = '\0';
   free(process->comm);
   process->comm = strdup(command);
   process->basenameOffset = tokenEnd;

   return true;
}

static bool LinuxProcessList_processEntries(ProcessList* this, const char* dirname, Process* parent, double period, struct timeval tv) {
   DIR* dir;
   struct dirent* entry;

   time_t curTime = tv.tv_sec;
   #ifdef HAVE_TASKSTATS
   unsigned long long now = tv.tv_sec*1000LL+tv.tv_usec/1000LL;
   #endif

   dir = opendir(dirname);
   if (!dir) return false;
   int cpus = this->cpuCount;
   bool hideKernelThreads = this->hideKernelThreads;
   bool hideUserlandThreads = this->hideUserlandThreads;
   while ((entry = readdir(dir)) != NULL) {
      char* name = entry->d_name;

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      if ((!this->hideThreads) && name[0] == '.') {
         name++;
      }

      // Just skip all non-number directories.
      if (name[0] < '0' || name[0] > '9') {
         continue;
      }

      // filename is a number: process directory
      int pid = atoi(name);
     
      if (parent && pid == parent->pid)
         continue;

      if (pid <= 0) 
         continue;

      Process* process = NULL;
      Process* existingProcess = (Process*) Hashtable_get(this->processTable, pid);

      if (existingProcess) {
         assert(Vector_indexOf(this->processes, existingProcess, Process_pidCompare) != -1);
         process = existingProcess;
         assert(process->pid == pid);
      } else {
         process = (Process*) LinuxProcess_new(settings, this);
         assert(process->comm == NULL);
         process->pid = pid;
         process->tgid = parent ? parent->pid : pid;
      }

      char subdirname[MAX_NAME+1];
      snprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      LinuxProcessList_processEntries(this, subdirname, process, period, tv);

      #ifdef HAVE_TASKSTATS
      if (this->flags & PROCESS_FLAG_IO)
         LinuxProcessList_readIoFile(process, dirname, name, now);
      #endif

      if (! LinuxProcessList_readStatmFile(process, dirname, name))
         goto errorReadingProcess;

      process->show = ! ((hideKernelThreads && Process_isKernelThread(process)) || (hideUserlandThreads && Process_isUserlandThread(process)));

      char command[MAX_NAME+1];
      unsigned long long int lasttimes = (process->utime + process->stime);
      if (! LinuxProcessList_readStatFile(process, dirname, name, command))
         goto errorReadingProcess;
      if (this->flags & PROCESS_FLAG_IOPRIO)
         LinuxProcess_updateIOPriority((LinuxProcess*)process);
      float percent_cpu = (process->utime + process->stime - lasttimes) / period * 100.0;
      process->percent_cpu = MAX(MIN(percent_cpu, cpus*100.0), 0.0);
      if (isnan(process->percent_cpu)) process->percent_cpu = 0.0;
      process->percent_mem = (process->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;

      if(!existingProcess) {

         if (! LinuxProcessList_statProcessDir(process, dirname, name, curTime))
            goto errorReadingProcess;

         process->user = UsersTable_getRef(this->usersTable, process->st_uid);

         #ifdef HAVE_OPENVZ
         LinuxProcessList_readOpenVZData(this, process, dirname, name);
         #endif
         
         #ifdef HAVE_VSERVER
         if (this->flags & PROCESS_FLAG_VSERVER)
            LinuxProcessList_readVServerData(process, dirname, name);
         #endif

         if (! LinuxProcessList_readCmdlineFile(process, dirname, name))
            goto errorReadingProcess;

         ProcessList_add(this, process);
      } else {
         if (this->updateProcessNames) {
            if (! LinuxProcessList_readCmdlineFile(process, dirname, name))
               goto errorReadingProcess;
         }
      }

      #ifdef HAVE_CGROUP
      if (this->flags & PROCESS_FLAG_CGROUP)
         LinuxProcessList_readCGroupFile(process, dirname, name);
      #endif
      
      #ifdef HAVE_OOM
      LinuxProcessList_readOomData(process, dirname, name);
      #endif

      if (process->state == 'Z') {
         free(process->comm);
         process->basenameOffset = -1;
         process->comm = strdup(command);
      } else if (Process_isThread(process)) {
         if (this->showThreadNames || Process_isKernelThread(process) || process->state == 'Z') {
            free(process->comm);
            process->basenameOffset = -1;
            process->comm = strdup(command);
         } else if (this->showingThreadNames) {
            if (! LinuxProcessList_readCmdlineFile(process, dirname, name))
               goto errorReadingProcess;
         }
         if (Process_isKernelThread(process)) {
            this->kernelThreads++;
         } else {
            this->userlandThreads++;
         }
      }

      this->totalTasks++;
      if (process->state == 'R')
         this->runningTasks++;
      process->updated = true;

      continue;

      // Exception handler.
      errorReadingProcess: {
         if (process->comm) {
            free(process->comm);
            process->basenameOffset = -1;
            process->comm = NULL;
         }
         if (existingProcess)
            ProcessList_remove(this, process);
         else
            LinuxProcess_delete((Object*)process);
      }
   }
   closedir(dir);
   return true;
}

static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this) {
   unsigned long long int swapFree = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {

      switch (buffer[0]) {
      case 'M':
         if (String_startsWith(buffer, "MemTotal:"))
            sscanf(buffer, "MemTotal: %32llu kB", &this->totalMem);
         else if (String_startsWith(buffer, "MemFree:"))
            sscanf(buffer, "MemFree: %32llu kB", &this->freeMem);
         else if (String_startsWith(buffer, "MemShared:"))
            sscanf(buffer, "MemShared: %32llu kB", &this->sharedMem);
         break;
      case 'B':
         if (String_startsWith(buffer, "Buffers:"))
            sscanf(buffer, "Buffers: %32llu kB", &this->buffersMem);
         break;
      case 'C':
         if (String_startsWith(buffer, "Cached:"))
            sscanf(buffer, "Cached: %32llu kB", &this->cachedMem);
         break;
      case 'S':
         if (String_startsWith(buffer, "SwapTotal:"))
            sscanf(buffer, "SwapTotal: %32llu kB", &this->totalSwap);
         if (String_startsWith(buffer, "SwapFree:"))
            sscanf(buffer, "SwapFree: %32llu kB", &swapFree);
         break;
      }
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(file);
}

static inline double LinuxProcessList_scanCPUTime(ProcessList* this) {
   unsigned long long int usertime, nicetime, systemtime, idletime;

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   int cpus = this->cpuCount;
   assert(cpus > 0);
   for (int i = 0; i <= cpus; i++) {
      char buffer[256];
      int cpuid;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      unsigned long long int systemalltime, idlealltime, totaltime, virtalltime;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Dependending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      fgets(buffer, 255, file);
      if (i == 0)
         sscanf(buffer, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      else {
         sscanf(buffer, "cpu%4d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         assert(cpuid == i - 1);
      }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      idlealltime = idletime + ioWait;
      systemalltime = systemtime + irq + softIrq;
      virtalltime = guest + guestnice;
      totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(this->cpus[i]);
      assert (usertime >= cpuData->userTime);
      assert (nicetime >= cpuData->niceTime);
      assert (systemtime >= cpuData->systemTime);
      assert (idletime >= cpuData->idleTime);
      assert (totaltime >= cpuData->totalTime);
      assert (systemalltime >= cpuData->systemAllTime);
      assert (idlealltime >= cpuData->idleAllTime);
      assert (ioWait >= cpuData->ioWaitTime);
      assert (irq >= cpuData->irqTime);
      assert (softIrq >= cpuData->softIrqTime);
      assert (steal >= cpuData->stealTime);
      assert (virtalltime >= cpuData->guestTime);
      cpuData->userPeriod = usertime - cpuData->userTime;
      cpuData->nicePeriod = nicetime - cpuData->niceTime;
      cpuData->systemPeriod = systemtime - cpuData->systemTime;
      cpuData->systemAllPeriod = systemalltime - cpuData->systemAllTime;
      cpuData->idleAllPeriod = idlealltime - cpuData->idleAllTime;
      cpuData->idlePeriod = idletime - cpuData->idleTime;
      cpuData->ioWaitPeriod = ioWait - cpuData->ioWaitTime;
      cpuData->irqPeriod = irq - cpuData->irqTime;
      cpuData->softIrqPeriod = softIrq - cpuData->softIrqTime;
      cpuData->stealPeriod = steal - cpuData->stealTime;
      cpuData->guestPeriod = virtalltime - cpuData->guestTime;
      cpuData->totalPeriod = totaltime - cpuData->totalTime;
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }
   double period = (double)this->cpus[0].totalPeriod / cpus; fclose(file);
   return period;
}

void ProcessList_scan(ProcessList* this) {

   LinuxProcessList_scanMemoryInfo(this);
   
   double period = LinuxProcessList_scanCPUTime(this);

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(this->processes); i++) {
      Process* p = (Process*) Vector_get(this->processes, i);
      p->updated = false;
   }
   
   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   LinuxProcessList_processEntries(this, PROCDIR, NULL, period, tv);
   
   this->showingThreadNames = this->showThreadNames;
   
   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      if (p->updated == false)
         ProcessList_remove(this, p);
      else
         p->updated = false;
   }

}

