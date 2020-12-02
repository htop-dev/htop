/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "CRT.h"
#include "Hashtable.h"
#include "Vector.h"
#include "XUtils.h"


ProcessList* ProcessList_init(ProcessList* this, const ObjectClass* klass, UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   this->processes = Vector_new(klass, true, DEFAULT_SIZE);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidMatchList = pidMatchList;
   this->userId = userId;

   // tree-view auxiliary buffer
   this->processes2 = Vector_new(klass, true, DEFAULT_SIZE);

   this->displayTreeSet = Hashtable_new(4096, false);
   this->draftingTreeSet = Hashtable_new(4096, false);

   // set later by platform-specific code
   this->cpuCount = 0;

   this->scanTs = 0;

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   if (hwloc_topology_init(&this->topology) == 0) {
      this->topologyOk =
         #if HWLOC_API_VERSION < 0x00020000
         /* try to ignore the top-level machine object type */
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_MACHINE) &&
         /* ignore caches, which don't add structure */
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_CORE) &&
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_CACHE) &&
         0 == hwloc_topology_set_flags(this->topology, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM) &&
         #else
         0 == hwloc_topology_set_all_types_filter(this->topology, HWLOC_TYPE_FILTER_KEEP_STRUCTURE) &&
         #endif
         0 == hwloc_topology_load(this->topology);
   }
#endif

   this->following = -1;

   return this;
}

void ProcessList_done(ProcessList* this) {
#ifdef HAVE_LIBHWLOC
   if (this->topologyOk) {
      hwloc_topology_destroy(this->topology);
   }
#endif
   Hashtable_delete(this->displayTreeSet);
   Hashtable_delete(this->draftingTreeSet);
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

void ProcessList_printHeader(ProcessList* this, RichString* header) {
   RichString_prune(header);
   const ProcessField* fields = this->settings->fields;
   for (int i = 0; fields[i]; i++) {
      const char* field = Process_fields[fields[i]].title;
      if (!field) {
         field = "- ";
      }

      int color = (this->settings->sortKey == fields[i]) ?
         CRT_colors[PANEL_SELECTION_FOCUS] : CRT_colors[PANEL_HEADER_FOCUS];
      RichString_append(header, color, field);
      if (COMM == fields[i] && this->settings->showMergedCommand) {
         RichString_append(header, color, "(merged)");
      }
   }
}

void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   p->processList = this;

   // highlighting processes found in first scan by first scan marked "far in the past"
   p->seenTs = this->scanTs;

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

   if (idx >= 0) {
      Vector_remove(this->processes, idx);
   }

   assert(Hashtable_get(this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

Process* ProcessList_get(ProcessList* this, int idx) {
   return (Process*) (Vector_get(this->processes, idx));
}

int ProcessList_size(ProcessList* this) {
   return (Vector_size(this->processes));
}

// ProcessList_updateTreeSetLayer sorts this->displayTreeSet,
// relying only on itself.
//
// Algorithm
//
// The algorithm is based on `depth-first search`,
// even though `breadth-first search` approach may be more efficient on first glance,
// after comparision it may be not, as it's not save to go deeper without first updating the tree structure.
// If it would be save that approach would likely bring an advantage in performance.
//
// Each call of the function looks for a 'layer'. A 'layer' is a list of processes with the same depth.
// First it sorts a list. Then it runs the function recursively for each element of the sorted list.
// After that it updates the settings of processes.
//
// It relies on `leftBound` and `rightBound` as an optimization to cut the list size at the time it builds a 'layer'.
//
// It uses a temporary Hashtable `draftingTreeSet` because it's not save to traverse a tree
// and at the same time make changes in it.
static void ProcessList_updateTreeSetLayer(ProcessList* this, unsigned int leftBound, unsigned int rightBound, unsigned int deep, unsigned int left, unsigned int right, unsigned int* index, unsigned int* treeIndex, int indent) {
   // It's guaranted that layer_size is enough space
   // but most likely it needs less. Specifically on first iteration.
   int layerSize = (right - left) / 2;

   // check if we reach `children` of `leafes`
   if (layerSize == 0)
      return;

   Vector* layer = Vector_new(this->processes->type, false, layerSize);

   // Find all processes on the same layer (process with the same `deep` value
   // and included in a range from `leftBound` to `rightBound`.
   //
   // This loop also keeps track of left_bound and right_bound of these processes
   // in order not to lose this information once the list is sorted.
   //
   // The variables left_bound and right_bound are different from what the values lhs and rhs represent,
   // While left_bound and right_bound define a range of processes to look at, the values given by lhs and rhs are indices into an array
   //
   // In the below example note how filtering a range of indices i is different from filtering for processes in the bounds left_bound < x < right_bound …
   //
   // The nested tree set is sorted by left value, which is guaranteed upon entry/exit of this function.
   //
   // i | l | r
   // 1 | 1 | 9
   // 2 | 2 | 8
   // 3 | 4 | 5
   // 4 | 6 | 7
   for (unsigned int i = leftBound; i < rightBound; i++) {
      Process* proc = (Process*)Hashtable_get(this->displayTreeSet, i);
      if (proc->tree_depth == deep && proc->tree_left > left && proc->tree_right < right) {
         if (Vector_size(layer) > 0) {
            Process* previous_process = (Process*)Vector_get(layer, Vector_size(layer)-1);
            // Make a 'right_bound' of previous_process in a layer a current's process index.
            //
            // Use 'tree_depth' as a temporal variable.
            // it is save to do as later 'tree_depth' will be renovated.
            previous_process->tree_depth = proc->tree_index;
         }
         Vector_add(layer, proc);
      }
   }

   // The loop above changes process-1 so the last process on the layer
   // isn't updated by the that loop.
   //
   // Thus, if present, set the `rightBound` to the last process on the layer
   if (Vector_size(layer) > 0) {
      Process* previous_process = (Process*)Vector_get(layer, Vector_size(layer)-1);
      previous_process->tree_depth = rightBound;
   }

   Vector_quickSort(layer);

   int size = Vector_size(layer);
   for (int i = 0; i < size; i++) {
      Process* proc = (Process*)Vector_get(layer, i);

      unsigned int idx = (*index)++;
      int newLeft = (*treeIndex)++;

      int level = deep == 0 ? 0 : (int)deep-1;
      int currentIndent = indent == -1 ? 0 : indent | (1 << level);
      int nextIndent = indent == -1 ? 0 : (i < size - 1) ? currentIndent : indent;

      unsigned int newLeftBound = proc->tree_index;
      unsigned int newRightBound = proc->tree_depth;
      ProcessList_updateTreeSetLayer(this, newLeftBound, newRightBound, deep+1, proc->tree_left, proc->tree_right, index, treeIndex, nextIndent);

      int newRight = (*treeIndex)++;

      proc->tree_left = newLeft;
      proc->tree_right = newRight;
      proc->tree_index = idx;
      proc->tree_depth = deep;

      if (indent == -1) {
         proc->indent = 0;
      } else if (i == size - 1) {
         proc->indent = -currentIndent;
      } else {
         proc->indent = currentIndent;
      }

      Hashtable_put(this->draftingTreeSet, proc->tree_index, proc);

      // It's not strictly necessary to do this, but doing so anyways
      // allows for checking the correctness of the inner workings.
      Hashtable_remove(this->displayTreeSet, newLeftBound);
   }

   Vector_delete(layer);
}

static void ProcessList_updateTreeSet(ProcessList* this) {
   unsigned int index = 0;
   unsigned int tree_index = 1;

   const int vsize = Vector_size(this->processes);

   assert(Hashtable_count(this->draftingTreeSet) == 0);
   assert((int)Hashtable_count(this->displayTreeSet) == vsize);

   ProcessList_updateTreeSetLayer(this, 0, vsize, 0, 0, vsize * 2 + 1, &index, &tree_index, -1);

   Hashtable* tmp = this->draftingTreeSet;
   this->draftingTreeSet = this->displayTreeSet;
   this->displayTreeSet = tmp;

   assert(Hashtable_count(this->draftingTreeSet) == 0);
   assert((int)Hashtable_count(this->displayTreeSet) == vsize);
}

static void ProcessList_buildTreeBranch(ProcessList* this, pid_t pid, int level, int indent, int direction, bool show, int* node_counter, int* node_index) {
   Vector* children = Vector_new(Class(Process), false, DEFAULT_SIZE);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(this->processes, i));
      if (process->show && Process_isChildOf(process, pid)) {
         process = (Process*) (Vector_take(this->processes, i));
         Vector_add(children, process);
      }
   }

   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      int index = (*node_index)++;
      Process* process = (Process*) (Vector_get(children, i));

      int lft = (*node_counter)++;

      if (!show) {
         process->show = false;
      }

      int s = Vector_size(this->processes2);
      if (direction == 1) {
         Vector_add(this->processes2, process);
      } else {
         Vector_insert(this->processes2, 0, process);
      }

      assert(Vector_size(this->processes2) == s+1); (void)s;

      int nextIndent = indent | (1 << level);
      ProcessList_buildTreeBranch(this, process->pid, level+1, (i < size - 1) ? nextIndent : indent, direction, show ? process->showChildren : false, node_counter, node_index);
      if (i == size - 1) {
         process->indent = -nextIndent;
      } else {
         process->indent = nextIndent;
      }

      int rht = (*node_counter)++;

      process->tree_left = lft;
      process->tree_right = rht;
      process->tree_depth = level+1;
      process->tree_index = index;
      Hashtable_put(this->displayTreeSet, index, process);
   }
   Vector_delete(children);
}

static long ProcessList_treeProcessCompare(const void* v1, const void* v2) {
   const Process *p1 = (const Process*)v1;
   const Process *p2 = (const Process*)v2;

   return SPACESHIP_NUMBER(p1->tree_left, p2->tree_left);
}

static long ProcessList_treeProcessCompareByPID(const void* v1, const void* v2) {
   const Process *p1 = (const Process*)v1;
   const Process *p2 = (const Process*)v2;

   return SPACESHIP_NUMBER(p1->pid, p2->pid);
}

static void ProcessList_buildTree(ProcessList* this) {
   int node_counter = 1;
   int node_index = 0;
   int direction = this->settings->direction;
   // Sort by PID
   Vector_quickSortCustomCompare(this->processes, ProcessList_treeProcessCompareByPID);
   int vsize = Vector_size(this->processes);
   // Find all processes whose parent is not visible
   int size;
   while ((size = Vector_size(this->processes))) {
      int i;
      for (i = 0; i < size; i++) {
         Process* process = (Process*)(Vector_get(this->processes, i));
         // Immediately consume not shown processes
         if (!process->show) {
            process = (Process*)(Vector_take(this->processes, i));
            process->indent = 0;
            process->tree_depth = 0;
            process->tree_left = (node_counter)++;
            process->tree_index = (node_index)++;
            Vector_add(this->processes2, process);
            ProcessList_buildTreeBranch(this, process->pid, 0, 0, direction, false, &node_counter, &node_index);
            process->tree_right = (node_counter)++;
            Hashtable_put(this->displayTreeSet, process->tree_index, process);
            break;
         }
         pid_t ppid = Process_getParentPid(process);
         // Bisect the process vector to find parent
         int l = 0, r = size;
         // If PID corresponds with PPID (e.g. "kernel_task" (PID:0, PPID:0)
         // on Mac OS X 10.11.6) cancel bisecting and regard this process as
         // root.
         if (process->pid == ppid)
            r = 0;
         while (l < r) {
            int c = (l + r) / 2;
            pid_t pid = ((Process*)(Vector_get(this->processes, c)))->pid;
            if (ppid == pid) {
               break;
            } else if (ppid < pid) {
               r = c;
            } else {
               l = c + 1;
            }
         }
         // If parent not found, then construct the tree with this root
         if (l >= r) {
            process = (Process*)(Vector_take(this->processes, i));
            process->indent = 0;
            process->tree_depth = 0;
            process->tree_left = (node_counter)++;
            process->tree_index = (node_index)++;
            Vector_add(this->processes2, process);
            Hashtable_put(this->displayTreeSet, process->tree_index, process);
            ProcessList_buildTreeBranch(this, process->pid, 0, 0, direction, process->showChildren, &node_counter, &node_index);
            process->tree_right = (node_counter)++;
            break;
         }
      }
      // There should be no loop in the process tree
      assert(i < size);
   }
   assert(Vector_size(this->processes2) == vsize); (void)vsize;
   assert(Vector_size(this->processes) == 0);
   // Swap listings around
   Vector* t = this->processes;
   this->processes = this->processes2;
   this->processes2 = t;
}

void ProcessList_sort(ProcessList* this) {
   if (!this->settings->treeView) {
      Vector_insertionSort(this->processes);
   } else {
      ProcessList_updateTreeSet(this);
      Vector_quickSortCustomCompare(this->processes, ProcessList_treeProcessCompare);
   }
}

ProcessField ProcessList_keyAt(const ProcessList* this, int at) {
   int x = 0;
   const ProcessField* fields = this->settings->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      const char* title = Process_fields[field].title;
      if (!title) {
         title = "- ";
      }

      int len = strlen(title);
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

void ProcessList_rebuildPanel(ProcessList* this) {
   const char* incFilter = this->incFilter;

   int currPos = Panel_getSelectedIndex(this->panel);
   pid_t currPid = this->following != -1 ? this->following : 0;
   int currScrollV = this->panel->scrollV;

   Panel_prune(this->panel);
   int size = ProcessList_size(this);
   int idx = 0;
   for (int i = 0; i < size; i++) {
      bool hidden = false;
      Process* p = ProcessList_get(this, i);

      if ( (!p->show)
         || (this->userId != (uid_t) -1 && (p->st_uid != this->userId))
         || (incFilter && !(String_contains_i(Process_getCommand(p), incFilter)))
         || (this->pidMatchList && !Hashtable_get(this->pidMatchList, p->tgid)) )
         hidden = true;

      if (!hidden) {
         Panel_set(this->panel, idx, (Object*)p);
         if ((this->following == -1 && idx == currPos) || (this->following != -1 && p->pid == currPid)) {
            Panel_setSelected(this->panel, idx);
            this->panel->scrollV = currScrollV;
         }
         idx++;
      }
   }
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor) {
   Process* proc = (Process*) Hashtable_get(this->processTable, pid);
   *preExisting = proc;
   if (proc) {
      assert(Vector_indexOf(this->processes, proc, Process_pidCompare) != -1);
      assert(proc->pid == pid);
   } else {
      proc = constructor(this->settings);
      assert(proc->comm == NULL);
      proc->pid = pid;
   }
   return proc;
}

void ProcessList_scan(ProcessList* this, bool pauseProcessUpdate) {
   struct timespec now;

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      ProcessList_goThroughEntries(this, true);
      return;
   }

   // mark all process as "dirty"
   for (int i = 0; i < Vector_size(this->processes); i++) {
      Process* p = (Process*) Vector_get(this->processes, i);
      p->updated = false;
      p->wasShown = p->show;
      p->show = true;
   }

   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;


   // set scanTs
   static bool firstScanDone = false;
   if (!firstScanDone) {
      this->scanTs = 0;
      firstScanDone = true;
   } else if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
      this->scanTs = now.tv_sec;
   }

   ProcessList_goThroughEntries(this, false);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      if (p->tombTs > 0) {
         // remove tombed process
         if (this->scanTs >= p->tombTs) {
            ProcessList_remove(this, p);
         }
      } else if (p->updated == false) {
         // process no longer exists
         if (this->settings->highlightChanges && p->wasShown) {
            // mark tombed
            p->tombTs = this->scanTs + this->settings->highlightDelaySecs;
         } else {
            // immediately remove
            ProcessList_remove(this, p);
         }
      } else {
         p->updated = false;
      }
   }

   // Clear out the hashtable to avoid any left-over processes from previous build
   //
   // The sorting algorithm relies on the fact that
   // len(this->displayTreeSet) == len(this->processes)
   Hashtable_clear(this->displayTreeSet);

   ProcessList_buildTree(this);
}
