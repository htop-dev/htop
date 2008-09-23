/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifndef CONFIG_H
#define CONFIG_H
#include "config.h"
#endif

#include "ProcessList.h"
#include "Process.h"
#include "Vector.h"
#include "UsersTable.h"
#include "Hashtable.h"
#include "String.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/utsname.h>
#include <stdarg.h>

#include "debug.h"
#include <assert.h>

/*{
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

#ifndef PER_PROCESSOR_FIELDS
#define PER_PROCESSOR_FIELDS 22
#endif

}*/

/*{

#ifdef DEBUG_PROC
typedef int(*vxscanf)(void*, const char*, va_list);
#endif

typedef struct ProcessList_ {
   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   Process* prototype;
   UsersTable* usersTable;

   int processorCount;
   int totalTasks;
   int runningTasks;

   // Must match number of PER_PROCESSOR_FIELDS constant
   unsigned long long int* totalTime;
   unsigned long long int* userTime;
   unsigned long long int* systemTime;
   unsigned long long int* systemAllTime;
   unsigned long long int* idleAllTime;
   unsigned long long int* idleTime;
   unsigned long long int* niceTime;
   unsigned long long int* ioWaitTime;
   unsigned long long int* irqTime;
   unsigned long long int* softIrqTime;
   unsigned long long int* stealTime;
   unsigned long long int* totalPeriod;
   unsigned long long int* userPeriod;
   unsigned long long int* systemPeriod;
   unsigned long long int* systemAllPeriod;
   unsigned long long int* idleAllPeriod;
   unsigned long long int* idlePeriod;
   unsigned long long int* nicePeriod;
   unsigned long long int* ioWaitPeriod;
   unsigned long long int* irqPeriod;
   unsigned long long int* softIrqPeriod;
   unsigned long long int* stealPeriod;

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
   bool hideKernelThreads;
   bool hideUserlandThreads;
   bool treeView;
   bool highlightBaseName;
   bool highlightMegabytes;
   bool highlightThreads;
   bool detailedCPUTime;
   #ifdef DEBUG_PROC
   FILE* traceFile;
   #endif

} ProcessList;
}*/

static ProcessField defaultHeaders[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

#ifdef DEBUG_PROC

#define ProcessList_read(this, buffer, format, ...) ProcessList_xread(this, (vxscanf) vsscanf, buffer, format, ## __VA_ARGS__ )
#define ProcessList_fread(this, file, format, ...)  ProcessList_xread(this, (vxscanf) vfscanf, file, format, ## __VA_ARGS__ )

static FILE* ProcessList_fopen(ProcessList* this, const char* path, const char* mode) {
   fprintf(this->traceFile, "[%s]\n", path);
   return fopen(path, mode);
}

static inline int ProcessList_xread(ProcessList* this, vxscanf fn, void* buffer, char* format, ...) {
   va_list ap;
   va_start(ap, format);
   int num = fn(buffer, format, ap);
   va_end(format);
   va_start(ap, format);
   while (*format) {
      char ch = *format;
      char* c; int* d;
      long int* ld; unsigned long int* lu;
      long long int* lld; unsigned long long int* llu;
      char** s;
      if (ch != '%') {
         fprintf(this->traceFile, "%c", ch);
         format++;
         continue;
      }
      format++;
      switch(*format) {
      case 'c': c = va_arg(ap, char*);  fprintf(this->traceFile, "%c", *c); break;
      case 'd': d = va_arg(ap, int*);   fprintf(this->traceFile, "%d", *d); break;
      case 's': s = va_arg(ap, char**); fprintf(this->traceFile, "%s", *s); break;
      case 'l':
         format++;
         switch (*format) {
         case 'd': ld = va_arg(ap, long int*); fprintf(this->traceFile, "%ld", *ld); break;
         case 'u': lu = va_arg(ap, unsigned long int*); fprintf(this->traceFile, "%lu", *lu); break;
         case 'l':
            format++;
            switch (*format) {
            case 'd': lld = va_arg(ap, long long int*); fprintf(this->traceFile, "%lld", *lld); break;
            case 'u': llu = va_arg(ap, unsigned long long int*); fprintf(this->traceFile, "%llu", *llu); break;
            }
         }
      }
      format++;
   }
   fprintf(this->traceFile, "\n");
   va_end(format);
   return num;
}

#else

#ifndef ProcessList_read
#define ProcessList_fopen(this, path, mode) fopen(path, mode)
#define ProcessList_read(this, buffer, format, ...) sscanf(buffer, format, ## __VA_ARGS__ )
#define ProcessList_fread(this, file, format, ...) fscanf(file, format, ## __VA_ARGS__ )
#endif

#endif

static inline void ProcessList_allocatePerProcessorBuffers(ProcessList* this, int procs) {
   unsigned long long int** bufferPtr = &(this->totalTime);
   unsigned long long int* buffer = calloc(procs * PER_PROCESSOR_FIELDS, sizeof(unsigned long long int));
   for (int i = 0; i < PER_PROCESSOR_FIELDS; i++) {
      *bufferPtr = buffer;
      bufferPtr++;
      buffer += procs;
   }
}

ProcessList* ProcessList_new(UsersTable* usersTable) {
   ProcessList* this;
   this = malloc(sizeof(ProcessList));
   this->processes = Vector_new(PROCESS_CLASS, true, DEFAULT_SIZE, Process_compare);
   this->processTable = Hashtable_new(70, false);
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
   this->prototype = Process_new(this);
   this->usersTable = usersTable;
   
   /* tree-view auxiliary buffers */
   this->processes2 = Vector_new(PROCESS_CLASS, true, DEFAULT_SIZE, Process_compare);
   
   #ifdef DEBUG_PROC
   this->traceFile = fopen("/tmp/htop-proc-trace", "w");
   #endif

   FILE* status = fopen(PROCSTATFILE, "r");
   assert(status != NULL);
   char buffer[256];
   int procs = -1;
   do {
      procs++;
      fgets(buffer, 255, status);
   } while (String_startsWith(buffer, "cpu"));
   fclose(status);
   this->processorCount = procs - 1;
   
   ProcessList_allocatePerProcessorBuffers(this, procs);

   for (int i = 0; i < procs; i++) {
      this->totalTime[i] = 1;
      this->totalPeriod[i] = 1;
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
   this->hideKernelThreads = false;
   this->hideUserlandThreads = false;
   this->treeView = false;
   this->highlightBaseName = false;
   this->highlightMegabytes = false;
   this->detailedCPUTime = false;

   return this;
}

void ProcessList_delete(ProcessList* this) {
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
   Process_delete((Object*)this->prototype);

   // Free first entry only;
   // other fields are offsets of the same buffer
   free(this->totalTime);

   #ifdef DEBUG_PROC
   fclose(this->traceFile);
   #endif

   free(this->fields);
   free(this);
}

void ProcessList_invertSortOrder(ProcessList* this) {
   if (this->direction == 1)
      this->direction = -1;
   else
      this->direction = 1;
}

RichString ProcessList_printHeader(ProcessList* this) {
   RichString out;
   RichString_initVal(out);
   ProcessField* fields = this->fields;
   for (int i = 0; fields[i]; i++) {
      char* field = Process_fieldTitles[fields[i]];
      if (this->sortKey == fields[i])
         RichString_append(&out, CRT_colors[PANEL_HIGHLIGHT_FOCUS], field);
      else
         RichString_append(&out, CRT_colors[PANEL_HEADER_FOCUS], field);
   }
   return out;
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
   int index = Vector_indexOf(this->processes, p, Process_pidCompare);
   assert(index != -1);
   Vector_remove(this->processes, index);
   assert(Hashtable_get(this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

Process* ProcessList_get(ProcessList* this, int index) {
   return (Process*) (Vector_get(this->processes, index));
}

int ProcessList_size(ProcessList* this) {
   return (Vector_size(this->processes));
}

static void ProcessList_buildTree(ProcessList* this, int pid, int level, int indent, int direction) {
   Vector* children = Vector_new(PROCESS_CLASS, false, DEFAULT_SIZE, Process_compare);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(this->processes, i));
      if (process->tgid == pid || (process->tgid == process->pid && process->ppid == pid)) {
         Process* process = (Process*) (Vector_take(this->processes, i));
         Vector_add(children, process);
      }
   }
   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) (Vector_get(children, i));
      int s = this->processes2->items;
      if (direction == 1)
         Vector_add(this->processes2, process);
      else
         Vector_insert(this->processes2, 0, process);
      assert(this->processes2->items == s+1); (void)s;
      int nextIndent = indent;
      if (i < size - 1)
         nextIndent = indent | (1 << level);
      ProcessList_buildTree(this, process->pid, level+1, nextIndent, direction);
      process->indent = indent | (1 << level);
   }
   Vector_delete(children);
}

void ProcessList_sort(ProcessList* this) {
   if (!this->treeView) {
      Vector_sort(this->processes);
   } else {
      // Save settings
      int direction = this->direction;
      int sortKey = this->sortKey;
      // Sort by PID
      this->sortKey = PID;
      this->direction = 1;
      Vector_sort(this->processes);
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
      ProcessList_buildTree(this, init->pid, 0, 0, direction);
      // Add leftovers
      while (Vector_size(this->processes)) {
         Process* p = (Process*) (Vector_take(this->processes, 0));
         p->indent = 0;
         Vector_add(this->processes2, p);
         ProcessList_buildTree(this, p->pid, 0, 0, direction);
      }
      assert(Vector_size(this->processes2) == vsize); (void)vsize;
      assert(Vector_size(this->processes) == 0);
      // Swap listings around
      Vector* t = this->processes;
      this->processes = this->processes2;
      this->processes2 = t;
   }
}

static int ProcessList_readStatFile(ProcessList* this, Process *proc, FILE *f, char *command) {
   static char buf[MAX_READ];
   unsigned long int zero;

   int size = fread(buf, 1, MAX_READ, f);
   if(!size) return 0;

   assert(proc->pid == atoi(buf));
   char *location = strchr(buf, ' ');
   if(!location) return 0;

   location += 2;
   char *end = strrchr(location, ')');
   if(!end) return 0;
   
   int commsize = end - location;
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   location = end + 2;
   
   #ifdef DEBUG_PROC
   int num = ProcessList_read(this, location, 
      "%c %u %u %u %u %d %lu %lu %lu %lu "
      "%lu %lu %lu %ld %ld %ld %ld %ld %ld "
      "%lu %lu %ld %lu %lu %lu %lu %lu "
      "%lu %lu %lu %lu %lu %lu %lu %lu "
      "%d %d",
      &proc->state, &proc->ppid, &proc->pgrp, &proc->session, &proc->tty_nr, 
      &proc->tpgid, &proc->flags,
      &proc->minflt, &proc->cminflt, &proc->majflt, &proc->cmajflt,
      &proc->utime, &proc->stime, &proc->cutime, &proc->cstime, 
      &proc->priority, &proc->nice, &proc->nlwp, &proc->itrealvalue,
      &proc->starttime, &proc->vsize, &proc->rss, &proc->rlim, 
      &proc->startcode, &proc->endcode, &proc->startstack, &proc->kstkesp, 
      &proc->kstkeip, &proc->signal, &proc->blocked, &proc->sigignore, 
      &proc->sigcatch, &proc->wchan, &proc->nswap, &proc->cnswap, 
      &proc->exit_signal, &proc->processor);
   #else
   long int uzero;
   int num = ProcessList_read(this, location, 
      "%c %u %u %u %u %d %lu %lu %lu %lu "
      "%lu %lu %lu %ld %ld %ld %ld %ld %ld "
      "%lu %lu %ld %lu %lu %lu %lu %lu "
      "%lu %lu %lu %lu %lu %lu %lu %lu "
      "%d %d",
      &proc->state, &proc->ppid, &proc->pgrp, &proc->session, &proc->tty_nr, 
      &proc->tpgid, &proc->flags,
      &zero, &zero, &zero, &zero,
      &proc->utime, &proc->stime, &proc->cutime, &proc->cstime, 
      &proc->priority, &proc->nice, &proc->nlwp, &uzero,
      &zero, &zero, &uzero, &zero, 
      &zero, &zero, &zero, &zero, 
      &zero, &zero, &zero, &zero, 
      &zero, &zero, &zero, &zero, 
      &proc->exit_signal, &proc->processor);
   #endif
   
   // This assert is always valid on 2.4, but reportedly not always valid on 2.6.
   // TODO: Check if the semantics of this field has changed.
   // assert(zero == 0);
   
   if(num != 37) return 0;
   return 1;
}

static bool ProcessList_readStatusFile(ProcessList* this, Process* proc, char* dirname, char* name) {
   char statusfilename[MAX_NAME+1];
   statusfilename[MAX_NAME] = '\0';

   snprintf(statusfilename, MAX_NAME, "%s/%s", dirname, name);
   struct stat sstat;
   int statok = stat(statusfilename, &sstat);
   if (statok == -1)
      return false;
   proc->st_uid = sstat.st_uid;
   return true;
}

#ifdef HAVE_TASKSTATS

static void ProcessList_readIoFile(ProcessList* this, Process* proc, char* dirname, char* name) {
   char iofilename[MAX_NAME+1];
   iofilename[MAX_NAME] = '\0';

   snprintf(iofilename, MAX_NAME, "%s/%s/io", dirname, name);
   FILE* io = ProcessList_fopen(this, iofilename, "r");
   if (io) {
      char buffer[256];
      buffer[255] = '\0';
      struct timeval tv;
      gettimeofday(&tv,NULL);
      unsigned long long now = tv.tv_sec*1000+tv.tv_usec/1000;
      unsigned long long last_read = proc->io_read_bytes;
      unsigned long long last_write = proc->io_write_bytes;
      while (!feof(io)) {
         char* ok = fgets(buffer, 255, io);
         if (!ok)
            break;
         if (ProcessList_read(this, buffer, "rchar: %llu", &proc->io_rchar)) continue;
         if (ProcessList_read(this, buffer, "wchar: %llu", &proc->io_wchar)) continue;
         if (ProcessList_read(this, buffer, "syscr: %llu", &proc->io_syscr)) continue;
         if (ProcessList_read(this, buffer, "syscw: %llu", &proc->io_syscw)) continue;
         if (ProcessList_read(this, buffer, "read_bytes: %llu", &proc->io_read_bytes)) {
            proc->io_rate_read_bps = 
               ((double)(proc->io_read_bytes - last_read))/(((double)(now - proc->io_rate_read_time))/1000);
            proc->io_rate_read_time = now;
            continue;
         }
         if (ProcessList_read(this, buffer, "write_bytes: %llu", &proc->io_write_bytes)) {
            proc->io_rate_write_bps = 
               ((double)(proc->io_write_bytes - last_write))/(((double)(now - proc->io_rate_write_time))/1000);
            proc->io_rate_write_time = now;
            continue;
         }
         ProcessList_read(this, buffer, "cancelled_write_bytes: %llu", &proc->io_cancelled_write_bytes);
      }
      fclose(io);
   }
}

#endif

static bool ProcessList_processEntries(ProcessList* this, char* dirname, Process* parent, float period) {
   DIR* dir;
   struct dirent* entry;
   Process* prototype = this->prototype;

   dir = opendir(dirname);
   if (!dir) return false;
   int processors = this->processorCount;
   bool showUserlandThreads = !this->hideUserlandThreads;
   while ((entry = readdir(dir)) != NULL) {
      char* name = entry->d_name;
      int pid;
      // filename is a number: process directory
      pid = atoi(name);

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      bool isThread = false;
      if ((!this->hideThreads) && pid == 0 && name[0] == '.') {
         char* tname = name + 1;
         pid = atoi(tname);
         if (pid > 0)
            isThread = true;
      }

      if (pid > 0) {

         FILE* status;
         char statusfilename[MAX_NAME+1];
         char command[PROCESS_COMM_LEN + 1];

         Process* process = NULL;
         Process* existingProcess = (Process*) Hashtable_get(this->processTable, pid);

         if (existingProcess) {
            assert(Vector_indexOf(this->processes, existingProcess, Process_pidCompare) != -1);
            process = existingProcess;
            assert(process->pid == pid);
         } else {
            if (parent && parent->pid == pid) {
               process = parent;
            } else {
               process = prototype;
               assert(process->comm == NULL);
               process->pid = pid;
            }
         }
         if (parent) {
            process->tgid = parent->pid;
         }

         if (showUserlandThreads && (!parent || pid != parent->pid)) {
            char subdirname[MAX_NAME+1];
            snprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
   
            if (ProcessList_processEntries(this, subdirname, process, period))
               continue;
         }

         #ifdef HAVE_TASKSTATS        
         ProcessList_readIoFile(this, process, dirname, name);
         #endif

         process->updated = true;

         if (!existingProcess)
            if (! ProcessList_readStatusFile(this, process, dirname, name))
               goto errorReadingProcess;

         snprintf(statusfilename, MAX_NAME, "%s/%s/statm", dirname, name);
         status = ProcessList_fopen(this, statusfilename, "r");

         if(!status) {
            goto errorReadingProcess;
         }
         int num = ProcessList_fread(this, status, "%d %d %d %d %d %d %d",
             &process->m_size, &process->m_resident, &process->m_share, 
             &process->m_trs, &process->m_lrs, &process->m_drs, 
             &process->m_dt);

         fclose(status);
         if(num != 7)
            goto errorReadingProcess;

         if (this->hideKernelThreads && process->m_size == 0)
            goto errorReadingProcess;

         int lasttimes = (process->utime + process->stime);

         snprintf(statusfilename, MAX_NAME, "%s/%s/stat", dirname, name);
         
         status = ProcessList_fopen(this, statusfilename, "r");
         if (status == NULL)
            goto errorReadingProcess;

         int success = ProcessList_readStatFile(this, process, status, command);
         fclose(status);
         if(!success) {
            goto errorReadingProcess;
         }

         if(!existingProcess) {
            process->user = UsersTable_getRef(this->usersTable, process->st_uid);

            #ifdef HAVE_OPENVZ
            if (access("/proc/vz", R_OK) != 0) {
               process->vpid = process->pid;
               process->veid = 0;
            } else {
               snprintf(statusfilename, MAX_NAME, "%s/%s/stat", dirname, name);
               status = ProcessList_fopen(this, statusfilename, "r");
               if (status == NULL) 
                  goto errorReadingProcess;
               num = ProcessList_fread(this, status, 
                  "%*u %*s %*c %*u %*u %*u %*u %*u %*u %*u "
                  "%*u %*u %*u %*u %*u %*u %*u %*u "
                  "%*u %*u %*u %*u %*u %*u %*u %*u "
                  "%*u %*u %*u %*u %*u %*u %*u %*u "
                  "%*u %*u %*u %*u %*u %*u %*u %*u "
                  "%*u %*u %*u %*u %*u %*u %*u "
                  "%u %u",
                  &process->vpid, &process->veid);
               fclose(status);
            }
            #endif

            #ifdef HAVE_VSERVER
            snprintf(statusfilename, MAX_NAME, "%s/%s/status", dirname, name);
            status = ProcessList_fopen(this, statusfilename, "r");
            if (status == NULL) 
               goto errorReadingProcess;
            else {
               char buffer[256];
               process->vxid = 0;
               while (!feof(status)) {
                  char* ok = fgets(buffer, 255, status);
                  if (!ok)
                     break;

                  if (String_startsWith(buffer, "VxID:")) {
                     int vxid;
                     int ok = ProcessList_read(this, buffer, "VxID:\t%d", &vxid);
                     if (ok >= 1) {
                        process->vxid = vxid;
                     }
                  }
                  #if defined HAVE_ANCIENT_VSERVER
                  else if (String_startsWith(buffer, "s_context:")) {
                     int vxid;
                     int ok = ProcessList_read(this, buffer, "s_context:\t%d", &vxid);
                     if (ok >= 1) {
                        process->vxid = vxid;
                     }
                  }
                  #endif
               }
               fclose(status);
            }
            #endif
 
            snprintf(statusfilename, MAX_NAME, "%s/%s/cmdline", dirname, name);
            status = ProcessList_fopen(this, statusfilename, "r");
            if (!status) {
               goto errorReadingProcess;
            }

            int amtRead = fread(command, 1, PROCESS_COMM_LEN - 1, status);
            if (amtRead > 0) {
               for (int i = 0; i < amtRead; i++)
                  if (command[i] == '\0' || command[i] == '\n')
                     command[i] = ' ';
               command[amtRead] = '\0';
            }
            command[PROCESS_COMM_LEN] = '\0';
            process->comm = String_copy(command);
            fclose(status);
         }

         int percent_cpu = (process->utime + process->stime - lasttimes) / 
            period * 100.0;
         process->percent_cpu = MAX(MIN(percent_cpu, processors*100.0), 0.0);

         process->percent_mem = (process->m_resident * PAGE_SIZE) / 
            (float)(this->totalMem) * 
            100.0;

         this->totalTasks++;
         if (process->state == 'R') {
            this->runningTasks++;
         }

         if (!existingProcess) {
            process = Process_clone(process);
            ProcessList_add(this, process);
         }

         continue;

         // Exception handler.
         errorReadingProcess: {
            if (process->comm) {
               free(process->comm);
               process->comm = NULL;
            }
            if (existingProcess)
               ProcessList_remove(this, process);
            assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
         }
      }
   }
   closedir(dir);
   return true;
}

void ProcessList_scan(ProcessList* this) {
   unsigned long long int usertime, nicetime, systemtime, systemalltime, idlealltime, idletime, totaltime;
   unsigned long long int swapFree;

   FILE* status;
   char buffer[128];
   status = ProcessList_fopen(this, PROCMEMINFOFILE, "r");
   assert(status != NULL);
   int processors = this->processorCount;
   while (!feof(status)) {
      fgets(buffer, 128, status);

      switch (buffer[0]) {
      case 'M':
         if (String_startsWith(buffer, "MemTotal:"))
            ProcessList_read(this, buffer, "MemTotal: %llu kB", &this->totalMem);
         else if (String_startsWith(buffer, "MemFree:"))
            ProcessList_read(this, buffer, "MemFree: %llu kB", &this->freeMem);
         else if (String_startsWith(buffer, "MemShared:"))
            ProcessList_read(this, buffer, "MemShared: %llu kB", &this->sharedMem);
         break;
      case 'B':
         if (String_startsWith(buffer, "Buffers:"))
            ProcessList_read(this, buffer, "Buffers: %llu kB", &this->buffersMem);
         break;
      case 'C':
         if (String_startsWith(buffer, "Cached:"))
            ProcessList_read(this, buffer, "Cached: %llu kB", &this->cachedMem);
         break;
      case 'S':
         if (String_startsWith(buffer, "SwapTotal:"))
            ProcessList_read(this, buffer, "SwapTotal: %llu kB", &this->totalSwap);
         if (String_startsWith(buffer, "SwapFree:"))
            ProcessList_read(this, buffer, "SwapFree: %llu kB", &swapFree);
         break;
      }
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(status);

   status = ProcessList_fopen(this, PROCSTATFILE, "r");

   assert(status != NULL);
   for (int i = 0; i <= processors; i++) {
      char buffer[256];
      int cpuid;
      unsigned long long int ioWait, irq, softIrq, steal;
      ioWait = irq = softIrq = steal = 0;
      // Dependending on your kernel version,
      // 5, 7 or 8 of these fields will be set.
      // The rest will remain at zero.
      fgets(buffer, 255, status);
      if (i == 0)
         ProcessList_read(this, buffer, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal);
      else {
         ProcessList_read(this, buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal);
         assert(cpuid == i - 1);
      }
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      idlealltime = idletime + ioWait;
      systemalltime = systemtime + irq + softIrq + steal;
      totaltime = usertime + nicetime + systemalltime + idlealltime;
      assert (usertime >= this->userTime[i]);
      assert (nicetime >= this->niceTime[i]);
      assert (systemtime >= this->systemTime[i]);
      assert (idletime >= this->idleTime[i]);
      assert (totaltime >= this->totalTime[i]);
      assert (systemalltime >= this->systemAllTime[i]);
      assert (idlealltime >= this->idleAllTime[i]);
      assert (ioWait >= this->ioWaitTime[i]);
      assert (irq >= this->irqTime[i]);
      assert (softIrq >= this->softIrqTime[i]);
      assert (steal >= this->stealTime[i]);
      this->userPeriod[i] = usertime - this->userTime[i];
      this->nicePeriod[i] = nicetime - this->niceTime[i];
      this->systemPeriod[i] = systemtime - this->systemTime[i];
      this->systemAllPeriod[i] = systemalltime - this->systemAllTime[i];
      this->idleAllPeriod[i] = idlealltime - this->idleAllTime[i];
      this->idlePeriod[i] = idletime - this->idleTime[i];
      this->ioWaitPeriod[i] = ioWait - this->ioWaitTime[i];
      this->irqPeriod[i] = irq - this->irqTime[i];
      this->softIrqPeriod[i] = softIrq - this->softIrqTime[i];
      this->stealPeriod[i] = steal - this->stealTime[i];
      this->totalPeriod[i] = totaltime - this->totalTime[i];
      this->userTime[i] = usertime;
      this->niceTime[i] = nicetime;
      this->systemTime[i] = systemtime;
      this->systemAllTime[i] = systemalltime;
      this->idleAllTime[i] = idlealltime;
      this->idleTime[i] = idletime;
      this->ioWaitTime[i] = ioWait;
      this->irqTime[i] = irq;
      this->softIrqTime[i] = softIrq;
      this->stealTime[i] = steal;
      this->totalTime[i] = totaltime;
   }
   float period = (float)this->totalPeriod[0] / processors;
   fclose(status);

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(this->processes); i++) {
      Process* p = (Process*) Vector_get(this->processes, i);
      p->updated = false;
   }
   
   this->totalTasks = 0;
   this->runningTasks = 0;

   ProcessList_processEntries(this, PROCDIR, NULL, period);
   
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
