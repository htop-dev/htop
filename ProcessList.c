/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include "CRT.h"
#include "String.h"

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

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"
#include <sys/types.h>

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

#ifndef ProcessList_cpuId
#define ProcessList_cpuId(pl, cpu) ((pl)->countCPUsFromZero ? (cpu) : (cpu)+1)
#endif

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef enum TreeType_ {
   TREE_TYPE_AUTO,
   TREE_TYPE_ASCII,
   TREE_TYPE_UTF8,
} TreeType;

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int systemTime;
   unsigned long long int systemAllTime;
   unsigned long long int idleAllTime;
   unsigned long long int idleTime;
   unsigned long long int niceTime;
   unsigned long long int ioWaitTime;
   unsigned long long int irqTime;
   unsigned long long int softIrqTime;
   unsigned long long int stealTime;
   unsigned long long int guestTime;
   
   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int systemPeriod;
   unsigned long long int systemAllPeriod;
   unsigned long long int idleAllPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int nicePeriod;
   unsigned long long int ioWaitPeriod;
   unsigned long long int irqPeriod;
   unsigned long long int softIrqPeriod;
   unsigned long long int stealPeriod;
   unsigned long long int guestPeriod;
} CPUData;

typedef struct ProcessList_ {
   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   UsersTable* usersTable;

   Panel* panel;
   int following;
   bool userOnly;
   uid_t userId;
   bool filtering;
   const char* incFilter;
   Hashtable* pidWhiteList;

   int cpuCount;
   int totalTasks;
   int userlandThreads;
   int kernelThreads;
   int runningTasks;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif
   CPUData* cpus;

   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int sharedMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   unsigned long long int totalSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;

   ProcessField* fields;
   ProcessField sortKey;
   int direction;
   bool hideThreads;
   bool shadowOtherUsers;
   bool showThreadNames;
   bool showingThreadNames;
   bool hideKernelThreads;
   bool hideUserlandThreads;
   bool treeView;
   bool highlightBaseName;
   bool highlightMegabytes;
   bool highlightThreads;
   bool detailedCPUTime;
   bool countCPUsFromZero;
   bool updateProcessNames;
   const char **treeStr;

} ProcessList;

}*/

static ProcessField defaultHeaders[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

const char *ProcessList_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "`", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

const char *ProcessList_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* this;
   this = calloc(sizeof(ProcessList), 1);
   this->processes = Vector_new(PROCESS_CLASS, true, DEFAULT_SIZE, Process_compare);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidWhiteList = pidWhiteList;
   
   /* tree-view auxiliary buffers */
   this->processes2 = Vector_new(PROCESS_CLASS, true, DEFAULT_SIZE, Process_compare);
   
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
   this->cpuCount = cpus - 1;

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   int topoErr = hwloc_topology_init(&this->topology);
   if (topoErr == 0) {
      topoErr = hwloc_topology_load(this->topology);
      this->topologyOk = true;
   }
#endif
   this->cpus = calloc(sizeof(CPUData), cpus);

   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   this->fields = calloc(sizeof(ProcessField), LAST_PROCESSFIELD+1);
   // TODO: turn 'fields' into a Vector,
   // (and ProcessFields into proper objects).
   for (int i = 0; defaultHeaders[i]; i++) {
      this->fields[i] = defaultHeaders[i];
   }
   this->sortKey = PERCENT_CPU;
   this->direction = 1;
   this->hideThreads = false;
   this->shadowOtherUsers = false;
   this->showThreadNames = false;
   this->showingThreadNames = false;
   this->hideKernelThreads = false;
   this->hideUserlandThreads = false;
   this->treeView = false;
   this->highlightBaseName = false;
   this->highlightMegabytes = false;
   this->detailedCPUTime = false;
   this->countCPUsFromZero = false;
   this->updateProcessNames = false;
   this->treeStr = NULL;
   this->following = -1;

   return this;
}

void ProcessList_delete(ProcessList* this) {
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
   free(this->cpus);
   free(this->fields);
   free(this);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

void ProcessList_invertSortOrder(ProcessList* this) {
   if (this->direction == 1)
      this->direction = -1;
   else
      this->direction = 1;
}

void ProcessList_printHeader(ProcessList* this, RichString* header) {
   RichString_prune(header);
   ProcessField* fields = this->fields;
   for (int i = 0; fields[i]; i++) {
      const char* field = Process_fieldTitles[fields[i]];
      if (!this->treeView && this->sortKey == fields[i])
         RichString_append(header, CRT_colors[PANEL_HIGHLIGHT_FOCUS], field);
      else
         RichString_append(header, CRT_colors[PANEL_HEADER_FOCUS], field);
   }
}

static void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   
   Vector_add(this->processes, p);
   Hashtable_put(this->processTable, p->pid, p);
   
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

static void ProcessList_remove(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   Process* pp = Hashtable_remove(this->processTable, p->pid);
   assert(pp == p); (void)pp;
   unsigned int pid = p->pid;
   int idx = Vector_indexOf(this->processes, p, Process_pidCompare);
   assert(idx != -1);
   if (idx >= 0) Vector_remove(this->processes, idx);
   assert(Hashtable_get(this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

Process* ProcessList_get(ProcessList* this, int idx) {
   return (Process*) (Vector_get(this->processes, idx));
}

int ProcessList_size(ProcessList* this) {
   return (Vector_size(this->processes));
}

static void ProcessList_buildTree(ProcessList* this, pid_t pid, int level, int indent, int direction, bool show) {
   Vector* children = Vector_new(PROCESS_CLASS, false, DEFAULT_SIZE, Process_compare);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(this->processes, i));
      if (process->tgid == pid || (process->tgid == process->pid && process->ppid == pid)) {
         process = (Process*) (Vector_take(this->processes, i));
         Vector_add(children, process);
      }
   }
   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) (Vector_get(children, i));
      if (!show)
         process->show = false;
      int s = this->processes2->items;
      if (direction == 1)
         Vector_add(this->processes2, process);
      else
         Vector_insert(this->processes2, 0, process);
      assert(this->processes2->items == s+1); (void)s;
      int nextIndent = indent | (1 << level);
      ProcessList_buildTree(this, process->pid, level+1, (i < size - 1) ? nextIndent : indent, direction, show ? process->showChildren : false);
      if (i == size - 1)
         process->indent = -nextIndent;
      else
         process->indent = nextIndent;
   }
   Vector_delete(children);
}

void ProcessList_sort(ProcessList* this) {
   if (!this->treeView) {
      Vector_insertionSort(this->processes);
   } else {
      // Save settings
      int direction = this->direction;
      int sortKey = this->sortKey;
      // Sort by PID
      this->sortKey = PID;
      this->direction = 1;
      Vector_quickSort(this->processes);
      // Restore settings
      this->sortKey = sortKey;
      this->direction = direction;
      // Take PID 1 as root and add to the new listing
      int vsize = Vector_size(this->processes);
      Process* init = (Process*) (Vector_take(this->processes, 0));
      // This assertion crashes on hardened kernels.
      // I wonder how well tree view works on those systems.
      // assert(init->pid == 1);
      init->indent = 0;
      Vector_add(this->processes2, init);
      // Recursively empty list
      ProcessList_buildTree(this, init->pid, 0, 0, direction, true);
      // Add leftovers
      while (Vector_size(this->processes)) {
         Process* p = (Process*) (Vector_take(this->processes, 0));
         p->indent = 0;
         Vector_add(this->processes2, p);
         ProcessList_buildTree(this, p->pid, 0, 0, direction, p->showChildren);
      }
      assert(Vector_size(this->processes2) == vsize); (void)vsize;
      assert(Vector_size(this->processes) == 0);
      // Swap listings around
      Vector* t = this->processes;
      this->processes = this->processes2;
      this->processes2 = t;
   }
}

static bool ProcessList_readStatFile(Process *process, const char* dirname, const char* name, char* command) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return false;

   static char buf[MAX_READ];

   int size = fread(buf, 1, MAX_READ, file);
   if (!size) { fclose(file); return false; }

   assert(process->pid == atoi(buf));
   char *location = strchr(buf, ' ');
   if (!location) { fclose(file); return false; }

   location += 2;
   char *end = strrchr(location, ')');
   if (!end) { fclose(file); return false; }
   
   int commsize = end - location;
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   location = end + 2;

   int num = sscanf(location, 
      "%c %d %u %u %u "
      "%d %lu "
      "%*u %*u %*u %*u "
      "%llu %llu %llu %llu "
      "%ld %ld %ld "
      "%*d %*u %*u %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u "
      "%d %d",
      &process->state, &process->ppid, &process->pgrp, &process->session, &process->tty_nr, 
      &process->tpgid, &process->flags,
      &process->utime, &process->stime, &process->cutime, &process->cstime, 
      &process->priority, &process->nice, &process->nlwp,
      &process->exit_signal, &process->processor);
   fclose(file);
   return (num == 16);
}

static bool ProcessList_statProcessDir(Process* process, const char* dirname, char* name) {
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
   strftime(process->starttime_show, 7, ((ctime > time(NULL) - 86400) ? "%R " : "%b%d "), &date);
   
   return true;
}

#ifdef HAVE_TASKSTATS

static void ProcessList_readIoFile(Process* process, const char* dirname, char* name) {
   char filename[MAX_NAME+1];
   filename[MAX_NAME] = '\0';

   snprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   
   char buffer[256];
   buffer[255] = '\0';
   struct timeval tv;
   gettimeofday(&tv,NULL);
   unsigned long long now = tv.tv_sec*1000+tv.tv_usec/1000;
   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   while (fgets(buffer, 255, file)) {
      switch (buffer[0]) {
      case 'r':
         if (buffer[1] == 'c')
            sscanf(buffer, "rchar: %llu", &process->io_rchar);
         else if (sscanf(buffer, "read_bytes: %llu", &process->io_read_bytes)) {
            process->io_rate_read_bps = 
               ((double)(process->io_read_bytes - last_read))/(((double)(now - process->io_rate_read_time))/1000);
            process->io_rate_read_time = now;
         }
         break;
      case 'w':
         if (buffer[1] == 'c')
            sscanf(buffer, "wchar: %llu", &process->io_wchar);
         else if (sscanf(buffer, "write_bytes: %llu", &process->io_write_bytes)) {
            process->io_rate_write_bps = 
               ((double)(process->io_write_bytes - last_write))/(((double)(now - process->io_rate_write_time))/1000);
            process->io_rate_write_time = now;
         }
         break;
      case 's':
         if (buffer[5] == 'r')
            sscanf(buffer, "syscr: %llu", &process->io_syscr);
         else
            sscanf(buffer, "syscw: %llu", &process->io_syscw);
         break;
      case 'c':
         sscanf(buffer, "cancelled_write_bytes: %llu", &process->io_cancelled_write_bytes);
      }
   }
   fclose(file);
}

#endif

static bool ProcessList_readStatmFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/statm", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return false;

   int num = fscanf(file, "%32d %32d %32d %32d %32d %32d %32d",
       &process->m_size, &process->m_resident, &process->m_share, 
       &process->m_trs, &process->m_lrs, &process->m_drs, 
       &process->m_dt);
   fclose(file);
   return (num == 7);
}

#ifdef HAVE_OPENVZ

static void ProcessList_readOpenVZData(Process* process, const char* dirname, const char* name) {
   if (access("/proc/vz", R_OK) != 0) {
      process->vpid = process->pid;
      process->ctid = 0;
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

static void ProcessList_readCGroupFile(Process* process, const char* dirname, const char* name) {
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

static void ProcessList_readVServerData(Process* process, const char* dirname, const char* name) {
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
         int ok = sscanf(buffer, "VxID:\t%d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

static bool ProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   if (Process_isKernelThread(process))
      return true;

   char filename[MAX_NAME+1];
   snprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return false;
         
   char command[4096+1]; // max cmdline length on Linux
   int amtRead = fread(command, 1, sizeof(command) - 1, file);
   if (amtRead > 0) {
      for (int i = 0; i < amtRead; i++)
         if (command[i] == '\0' || command[i] == '\n') {
            command[i] = ' ';
         }
   }
   command[amtRead] = '\0';
   fclose(file);
   free(process->comm);
   process->comm = strdup(command);

   return true;
}


static bool ProcessList_processEntries(ProcessList* this, const char* dirname, Process* parent, double period) {
   DIR* dir;
   struct dirent* entry;

   dir = opendir(dirname);
   if (!dir) return false;
   int cpus = this->cpuCount;
   bool hideKernelThreads = this->hideKernelThreads;
   bool hideUserlandThreads = this->hideUserlandThreads;
   while ((entry = readdir(dir)) != NULL) {
      char* name = entry->d_name;
      // filename is a number: process directory
      int pid = atoi(name);
     
      if (parent && pid == parent->pid)
         continue;

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      if ((!this->hideThreads) && pid == 0 && name[0] == '.') {
         pid = atoi(name + 1);
      }
      if (pid <= 0) 
         continue;

      Process* process = NULL;
      Process* existingProcess = (Process*) Hashtable_get(this->processTable, pid);

      if (existingProcess) {
         assert(Vector_indexOf(this->processes, existingProcess, Process_pidCompare) != -1);
         process = existingProcess;
         assert(process->pid == pid);
      } else {
         process = Process_new(this);
         assert(process->comm == NULL);
         process->pid = pid;
         process->tgid = parent ? parent->pid : pid;
      }

      char subdirname[MAX_NAME+1];
      snprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      ProcessList_processEntries(this, subdirname, process, period);

      #ifdef HAVE_TASKSTATS
      ProcessList_readIoFile(process, dirname, name);
      #endif

      if (! ProcessList_readStatmFile(process, dirname, name))
         goto errorReadingProcess;

      process->show = ! ((hideKernelThreads && Process_isKernelThread(process)) || (hideUserlandThreads && Process_isUserlandThread(process)));

      char command[MAX_NAME+1];
      unsigned long long int lasttimes = (process->utime + process->stime);
      if (! ProcessList_readStatFile(process, dirname, name, command))
         goto errorReadingProcess;
      Process_updateIOPriority(process);
      float percent_cpu = (process->utime + process->stime - lasttimes) / period * 100.0;
      process->percent_cpu = MAX(MIN(percent_cpu, cpus*100.0), 0.0);
      if (isnan(process->percent_cpu)) process->percent_cpu = 0.0;
      process->percent_mem = (process->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;

      if(!existingProcess) {

         if (! ProcessList_statProcessDir(process, dirname, name))
            goto errorReadingProcess;

         process->user = UsersTable_getRef(this->usersTable, process->st_uid);

         #ifdef HAVE_OPENVZ
         ProcessList_readOpenVZData(process, dirname, name);
         #endif

         #ifdef HAVE_CGROUP
         ProcessList_readCGroupFile(process, dirname, name);
         #endif
         
         #ifdef HAVE_VSERVER
         ProcessList_readVServerData(process, dirname, name);
         #endif
         
         if (! ProcessList_readCmdlineFile(process, dirname, name))
            goto errorReadingProcess;

         ProcessList_add(this, process);
      } else {
         if (this->updateProcessNames) {
            if (! ProcessList_readCmdlineFile(process, dirname, name))
               goto errorReadingProcess;
         }
      }

      if (process->state == 'Z') {
         free(process->comm);
         process->comm = strdup(command);
      } else if (Process_isThread(process)) {
         if (this->showThreadNames || Process_isKernelThread(process) || process->state == 'Z') {
            free(process->comm);
            process->comm = strdup(command);
         } else if (this->showingThreadNames) {
            if (! ProcessList_readCmdlineFile(process, dirname, name))
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
            process->comm = NULL;
         }
         if (existingProcess)
            ProcessList_remove(this, process);
         else
            Process_delete((Object*)process);
      }
   }
   closedir(dir);
   return true;
}

void ProcessList_scan(ProcessList* this) {
   unsigned long long int usertime, nicetime, systemtime, systemalltime, idlealltime, idletime, totaltime, virtalltime;
   unsigned long long int swapFree = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   int cpus = this->cpuCount;
   {
      char buffer[128];
      while (fgets(buffer, 128, file)) {
   
         switch (buffer[0]) {
         case 'M':
            if (String_startsWith(buffer, "MemTotal:"))
               sscanf(buffer, "MemTotal: %llu kB", &this->totalMem);
            else if (String_startsWith(buffer, "MemFree:"))
               sscanf(buffer, "MemFree: %llu kB", &this->freeMem);
            else if (String_startsWith(buffer, "MemShared:"))
               sscanf(buffer, "MemShared: %llu kB", &this->sharedMem);
            break;
         case 'B':
            if (String_startsWith(buffer, "Buffers:"))
               sscanf(buffer, "Buffers: %llu kB", &this->buffersMem);
            break;
         case 'C':
            if (String_startsWith(buffer, "Cached:"))
               sscanf(buffer, "Cached: %llu kB", &this->cachedMem);
            break;
         case 'S':
            if (String_startsWith(buffer, "SwapTotal:"))
               sscanf(buffer, "SwapTotal: %llu kB", &this->totalSwap);
            if (String_startsWith(buffer, "SwapFree:"))
               sscanf(buffer, "SwapFree: %llu kB", &swapFree);
            break;
         }
      }
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(file);

   file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   for (int i = 0; i <= cpus; i++) {
      char buffer[256];
      int cpuid;
      unsigned long long int ioWait, irq, softIrq, steal, guest;
      ioWait = irq = softIrq = steal = guest = 0;
      // Dependending on your kernel version,
      // 5, 7 or 8 of these fields will be set.
      // The rest will remain at zero.
      fgets(buffer, 255, file);
      if (i == 0)
         sscanf(buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest);
      else {
         sscanf(buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest);
         assert(cpuid == i - 1);
      }
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      idlealltime = idletime + ioWait;
      systemalltime = systemtime + irq + softIrq;
      virtalltime = steal + guest;
      totaltime = usertime + nicetime + systemalltime + idlealltime + virtalltime;
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
      assert (guest >= cpuData->guestTime);
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
      cpuData->guestPeriod = guest - cpuData->guestTime;
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
      cpuData->guestTime = guest;
      cpuData->totalTime = totaltime;
   }
   double period = (double)this->cpus[0].totalPeriod / cpus; fclose(file);

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(this->processes); i++) {
      Process* p = (Process*) Vector_get(this->processes, i);
      p->updated = false;
   }
   
   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;

   ProcessList_processEntries(this, PROCDIR, NULL, period);
   
   this->showingThreadNames = this->showThreadNames;
   
   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      if (p->updated == false)
         ProcessList_remove(this, p);
      else
         p->updated = false;
   }

}

ProcessField ProcessList_keyAt(ProcessList* this, int at) {
   int x = 0;
   ProcessField* fields = this->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(Process_fieldTitles[field]);
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return COMM;
}

void ProcessList_expandTree(ProcessList* this) {
   int size = Vector_size(this->processes);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) Vector_get(this->processes, i);
      process->showChildren = true;
   }
}

void ProcessList_rebuildPanel(ProcessList* this, bool flags, int following, bool userOnly, uid_t userId, bool filtering, const char* incFilter) {
   if (!flags) {
      following = this->following;
      userOnly = this->userOnly;
      userId = this->userId;
      filtering = this->filtering;
      incFilter = this->incFilter;
   } else {
      this->following = following;
      this->userOnly = userOnly;
      this->userId = userId;
      this->filtering = filtering;
      this->incFilter = incFilter;
   }

   int currPos = Panel_getSelectedIndex(this->panel);
   pid_t currPid = following != -1 ? following : 0;
   int currScrollV = this->panel->scrollV;

   Panel_prune(this->panel);
   int size = ProcessList_size(this);
   int idx = 0;
   for (int i = 0; i < size; i++) {
      bool hidden = false;
      Process* p = ProcessList_get(this, i);

      if ( (!p->show)
         || (userOnly && (p->st_uid != userId))
         || (filtering && !(String_contains_i(p->comm, incFilter)))
         || (this->pidWhiteList && !Hashtable_get(this->pidWhiteList, p->pid)) )
         hidden = true;

      if (!hidden) {
         Panel_set(this->panel, idx, (Object*)p);
         if ((following == -1 && idx == currPos) || (following != -1 && p->pid == currPid)) {
            Panel_setSelected(this->panel, idx);
            this->panel->scrollV = currScrollV;
         }
         idx++;
      }
   }
}
