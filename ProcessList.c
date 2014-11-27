/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include "CRT.h"
#include "String.h"

#include <stdlib.h>
#include <string.h>

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"

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

ProcessList* ProcessList_new(UsersTable* ut, Hashtable* pidWhiteList);
void ProcessList_delete(ProcessList* pl);
void ProcessList_scan(ProcessList* pl);

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

ProcessList* ProcessList_init(ProcessList* this, UsersTable* usersTable, Hashtable* pidWhiteList) {
   this->processes = Vector_new(Class(Process), true, DEFAULT_SIZE);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidWhiteList = pidWhiteList;
   
   // tree-view auxiliary buffers
   this->processes2 = Vector_new(Class(Process), true, DEFAULT_SIZE);
   
   // set later by platform-specific code
   this->cpuCount = 0;
   this->cpus = NULL;

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

   this->fields = calloc(LAST_PROCESSFIELD+1, sizeof(ProcessField));
   // TODO: turn 'fields' into a Vector,
   // (and ProcessFields into proper objects).
   this->flags = 0;
   for (int i = 0; defaultHeaders[i]; i++) {
      this->fields[i] = defaultHeaders[i];
      this->flags |= Process_fieldFlags[defaultHeaders[i]];
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

void ProcessList_done(ProcessList* this) {
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
   free(this->cpus);
   free(this->fields);
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

void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   
   Vector_add(this->processes, p);
   Hashtable_put(this->processTable, p->pid, p);
   
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

void ProcessList_remove(ProcessList* this, Process* p) {
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
      if (!init) return;
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

void ProcessList_rebuildPanel(ProcessList* this, bool flags, int following, const char* incFilter) {
   if (!flags) {
      following = this->following;
      incFilter = this->incFilter;
   } else {
      this->following = following;
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
         || (this->userOnly && (p->st_uid != this->userId))
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

