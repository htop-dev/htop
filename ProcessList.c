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
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"

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
   const char **treeStr;
   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   UsersTable* usersTable;

   Panel* panel;
   int following;
   uid_t userId;
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

   int flags;
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
   bool accountGuestInCPUMeter;
   bool userOnly;

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

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* this;
   this = calloc(1, sizeof(ProcessList));
   this->processes = Vector_new(Class(Process), true, DEFAULT_SIZE);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidWhiteList = pidWhiteList;
   
   /* tree-view auxiliary buffers */
   this->processes2 = Vector_new(Class(Process), true, DEFAULT_SIZE);
   
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

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   int topoErr = hwloc_topology_init(&this->topology);
   if (topoErr == 0) {
      topoErr = hwloc_topology_load(this->topology);
   }
   if (topoErr == 0) {
      this->topologyOk = true;
   }
#endif
   this->cpus = calloc(cpus, sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   this->fields = calloc(LAST_PROCESSFIELD+1, sizeof(ProcessField));
   // TODO: turn 'fields' into a Vector,
   // (and ProcessFields into proper objects).
   this->flags = 0;
   for (int i = 0; defaultHeaders[i]; i++) {
      this->fields[i] = defaultHeaders[i];
      this->fields[i] |= Process_fieldFlags[defaultHeaders[i]];
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

   if (CRT_utf8)
      this->treeStr = CRT_utf8 ? ProcessList_treeStrUtf8 : ProcessList_treeStrAscii;

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
   Vector* children = Vector_new(Class(Process), false, DEFAULT_SIZE);

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
   assert(location == NULL);

   return true;
}

static bool ProcessList_statProcessDir(Process* process, const char* dirname, char* name, time_t curTime) {
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

static void ProcessList_readIoFile(Process* process, const char* dirname, char* name, unsigned long long now) {
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

static bool ProcessList_readStatmFile(Process* process, const char* dirname, const char* name) {
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

static void ProcessList_readOomData(Process* process, const char* dirname, const char* name) {
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

static bool ProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
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


static bool ProcessList_processEntries(ProcessList* this, const char* dirname, Process* parent, double period, struct timeval tv) {
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
         process = Process_new(this);
         assert(process->comm == NULL);
         process->pid = pid;
         process->tgid = parent ? parent->pid : pid;
      }

      char subdirname[MAX_NAME+1];
      snprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      ProcessList_processEntries(this, subdirname, process, period, tv);

      #ifdef HAVE_TASKSTATS
      if (this->flags & PROCESS_FLAG_IO)
         ProcessList_readIoFile(process, dirname, name, now);
      #endif

      if (! ProcessList_readStatmFile(process, dirname, name))
         goto errorReadingProcess;

      process->show = ! ((hideKernelThreads && Process_isKernelThread(process)) || (hideUserlandThreads && Process_isUserlandThread(process)));

      char command[MAX_NAME+1];
      unsigned long long int lasttimes = (process->utime + process->stime);
      if (! ProcessList_readStatFile(process, dirname, name, command))
         goto errorReadingProcess;
      if (this->flags & PROCESS_FLAG_IOPRIO)
         Process_updateIOPriority(process);
      float percent_cpu = (process->utime + process->stime - lasttimes) / period * 100.0;
      process->percent_cpu = MAX(MIN(percent_cpu, cpus*100.0), 0.0);
      if (isnan(process->percent_cpu)) process->percent_cpu = 0.0;
      process->percent_mem = (process->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;

      if(!existingProcess) {

         if (! ProcessList_statProcessDir(process, dirname, name, curTime))
            goto errorReadingProcess;

         process->user = UsersTable_getRef(this->usersTable, process->st_uid);

         #ifdef HAVE_OPENVZ
         if (this->flags & PROCESS_FLAG_OPENVZ)
            ProcessList_readOpenVZData(process, dirname, name);
         #endif

         #ifdef HAVE_CGROUP
         if (this->flags & PROCESS_FLAG_CGROUP)
            ProcessList_readCGroupFile(process, dirname, name);
         #endif
         
         #ifdef HAVE_VSERVER
         if (this->flags & PROCESS_FLAG_VSERVER)
            ProcessList_readVServerData(process, dirname, name);
         #endif
         
         #ifdef HAVE_OOM
         ProcessList_readOomData(process, dirname, name);
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
         process->basenameOffset = -1;
         process->comm = strdup(command);
      } else if (Process_isThread(process)) {
         if (this->showThreadNames || Process_isKernelThread(process) || process->state == 'Z') {
            free(process->comm);
            process->basenameOffset = -1;
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
            process->basenameOffset = -1;
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
   unsigned long long int usertime, nicetime, systemtime, idletime;
   unsigned long long int swapFree = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   int cpus = this->cpuCount;
   assert(cpus > 0);
   {
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
   ProcessList_processEntries(this, PROCDIR, NULL, period, tv);
   
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

void ProcessList_rebuildPanel(ProcessList* this, bool flags, int following, bool userOnly, uid_t userId, const char* incFilter) {
   if (!flags) {
      following = this->following;
      userOnly = this->userOnly;
      userId = this->userId;
      incFilter = this->incFilter;
   } else {
      this->following = following;
      this->userOnly = userOnly;
      this->userId = userId;
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
         || (incFilter && !(String_contains_i(p->comm, incFilter)))
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
