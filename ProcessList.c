/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Platform.h"
#include "Vector.h"
#include "XUtils.h"


ProcessList* ProcessList_init(ProcessList* this, const ObjectClass* klass, UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId) {
   this->processes = Vector_new(klass, true, DEFAULT_SIZE);
   this->displayList = Vector_new(klass, false, DEFAULT_SIZE);

   this->processTable = Hashtable_new(200, false);
   this->needsSort = true;

   this->usersTable = usersTable;
   this->pidMatchList = pidMatchList;
   this->dynamicMeters = dynamicMeters;
   this->dynamicColumns = dynamicColumns;

   this->userId = userId;

   // set later by platform-specific code
   this->activeCPUs = 0;
   this->existingCPUs = 0;
   this->monotonicMs = 0;

   // always maintain valid realtime timestamps
   Platform_gettime_realtime(&this->realtime, &this->realtimeMs);

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

   Hashtable_delete(this->processTable);

   Vector_delete(this->displayList);
   Vector_delete(this->processes);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

static const char* alignedDynamicColumnTitle(const ProcessList* this, int key, char* titleBuffer, size_t titleBufferSize) {
   const DynamicColumn* column = Hashtable_get(this->dynamicColumns, key);
   if (column == NULL)
      return "- ";
   int width = column->width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   xSnprintf(titleBuffer, titleBufferSize, "%*s", width, column->heading);
   return titleBuffer;
}

static const char* alignedProcessFieldTitle(const ProcessList* this, ProcessField field) {
   static char titleBuffer[UINT8_MAX + sizeof(" ")];
   assert(sizeof(titleBuffer) >= DYNAMIC_MAX_COLUMN_WIDTH + sizeof(" "));
   assert(sizeof(titleBuffer) >= PROCESS_MAX_PID_DIGITS + sizeof(" "));
   assert(sizeof(titleBuffer) >= PROCESS_MAX_UID_DIGITS + sizeof(" "));

   if (field >= LAST_PROCESSFIELD)
      return alignedDynamicColumnTitle(this, field, titleBuffer, sizeof(titleBuffer));

   const char* title = Process_fields[field].title;
   if (!title)
      return "- ";

   if (Process_fields[field].pidColumn) {
      xSnprintf(titleBuffer, sizeof(titleBuffer), "%*s ", Process_pidDigits, title);
      return titleBuffer;
   }

   if (field == ST_UID) {
      xSnprintf(titleBuffer, sizeof(titleBuffer), "%*s ", Process_uidDigits, title);
      return titleBuffer;
   }

   if (Process_fields[field].autoWidth) {
      if (field == PERCENT_CPU)
         xSnprintf(titleBuffer, sizeof(titleBuffer), "%*s ", Process_fieldWidths[field], title);
      else
         xSnprintf(titleBuffer, sizeof(titleBuffer), "%-*.*s ", Process_fieldWidths[field], Process_fieldWidths[field], title);
      return titleBuffer;
   }

   return title;
}

void ProcessList_printHeader(const ProcessList* this, RichString* header) {
   RichString_rewind(header, RichString_size(header));

   const Settings* settings = this->settings;
   const ScreenSettings* ss = settings->ss;
   const ProcessField* fields = ss->fields;

   ProcessField key = ScreenSettings_getActiveSortKey(ss);

   for (int i = 0; fields[i]; i++) {
      int color;
      if (ss->treeView && ss->treeViewAlwaysByPID) {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      } else if (key == fields[i]) {
         color = CRT_colors[PANEL_SELECTION_FOCUS];
      } else {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      }

      RichString_appendWide(header, color, alignedProcessFieldTitle(this, fields[i]));
      if (key == fields[i] && RichString_getCharVal(*header, RichString_size(header) - 1) == ' ') {
         bool ascending = ScreenSettings_getActiveDirection(ss) == 1;
         RichString_rewind(header, 1);  // rewind to override space
         RichString_appendnWide(header,
                                CRT_colors[PANEL_SELECTION_FOCUS],
                                CRT_treeStr[ascending ? TREE_STR_ASC : TREE_STR_DESC],
                                1);
      }
      if (COMM == fields[i] && settings->showMergedCommand) {
         RichString_appendAscii(header, color, "(merged)");
      }
   }
}

void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidEqualCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   p->processList = this;

   // highlighting processes found in first scan by first scan marked "far in the past"
   p->seenStampMs = this->monotonicMs;

   Vector_add(this->processes, p);
   Hashtable_put(this->processTable, p->pid, p);

   assert(Vector_indexOf(this->processes, p, Process_pidEqualCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   assert(Vector_countEquals(this->processes, Hashtable_count(this->processTable)));
}

// ProcessList_removeIndex removes Process p from the list's map and soft deletes
// it from its vector. Vector_compact *must* be called once the caller is done
// removing items.
// Should only be called from ProcessList_scan to avoid breaking dying process highlighting.
static void ProcessList_removeIndex(ProcessList* this, const Process* p, int idx) {
   pid_t pid = p->pid;

   assert(p == (Process*)Vector_get(this->processes, idx));
   assert(Hashtable_get(this->processTable, pid) != NULL);

   Hashtable_remove(this->processTable, pid);
   Vector_softRemove(this->processes, idx);

   if (this->following != -1 && this->following == pid) {
      this->following = -1;
      Panel_setSelectionColor(this->panel, PANEL_SELECTION_FOCUS);
   }

   assert(Hashtable_get(this->processTable, pid) == NULL);
   assert(Vector_countEquals(this->processes, Hashtable_count(this->processTable)));
}

static void ProcessList_buildTreeBranch(ProcessList* this, pid_t pid, int level, int indent, bool show) {
   // On OpenBSD the kernel thread 'swapper' has pid 0.
   // Do not treat it as root of any tree.
   if (pid == 0)
      return;

   // The vector is sorted by parent PID, find the start of the range by bisection
   int vsize = Vector_size(this->processes);
   int l = 0;
   int r = vsize;
   while (l < r) {
      int c = (l + r) / 2;
      Process* process = (Process*)Vector_get(this->processes, c);
      pid_t ppid = process->isRoot ? 0 : Process_getParentPid(process);
      if (ppid < pid) {
         l = c + 1;
      } else {
         r = c;
      }
   }
   // Find the end to know the last line for indent handling purposes
   int lastShown = r;
   while (r < vsize) {
      Process* process = (Process*)Vector_get(this->processes, r);
      if (!Process_isChildOf(process, pid))
         break;
      if (process->show)
         lastShown = r;
      r++;
   }

   for (int i = l; i < r; i++) {
      Process* process = (Process*)Vector_get(this->processes, i);

      if (!show) {
         process->show = false;
      }

      Vector_add(this->displayList, process);

      int nextIndent = indent | (1 << level);
      ProcessList_buildTreeBranch(this, process->pid, level + 1, (i < lastShown) ? nextIndent : indent, process->show && process->showChildren);
      if (i == lastShown) {
         process->indent = -nextIndent;
      } else {
         process->indent = nextIndent;
      }

      process->tree_depth = level + 1;
   }
}

static int compareProcessByKnownParentThenNatural(const void* v1, const void* v2) {
   const Process* p1 = (const Process*)v1;
   const Process* p2 = (const Process*)v2;

   int result = SPACESHIP_NUMBER(
      p1->isRoot ? 0 : Process_getParentPid(p1),
      p2->isRoot ? 0 : Process_getParentPid(p2)
   );

   if (result != 0)
      return result;

   return Process_compare(v1, v2);
}

// Builds a sorted tree from scratch, without relying on previously gathered information
static void ProcessList_buildTree(ProcessList* this) {
   Vector_prune(this->displayList);

   // Mark root processes
   int vsize = Vector_size(this->processes);
   for (int i = 0; i < vsize; i++) {
      Process* process = (Process*)Vector_get(this->processes, i);
      pid_t ppid = Process_getParentPid(process);
      process->isRoot = false;

      // If PID corresponds with PPID (e.g. "kernel_task" (PID:0, PPID:0)
      // on Mac OS X 10.11.6) regard this process as root.
      if (process->pid == ppid) {
         process->isRoot = true;
         continue;
      }

      // On Linux both the init process (pid 1) and the root UMH kernel thread (pid 2)
      // use a ppid of 0. As that PID can't exist, we can skip searching for it.
      if (!ppid) {
         process->isRoot = true;
         continue;
      }

      // We don't know about its parent for whatever reason
      if (ProcessList_findProcess(this, ppid) == NULL)
         process->isRoot = true;
   }

   // Sort by known parent PID (roots first), then PID
   Vector_quickSortCustomCompare(this->processes, compareProcessByKnownParentThenNatural);

   // Find all processes whose parent is not visible
   for (int i = 0; i < vsize; i++) {
      Process* process = (Process*)Vector_get(this->processes, i);

      // If parent not found, then construct the tree with this node as root
      if (process->isRoot) {
         process = (Process*)Vector_get(this->processes, i);
         process->indent = 0;
         process->tree_depth = 0;
         Vector_add(this->displayList, process);
         ProcessList_buildTreeBranch(this, process->pid, 0, 0, process->showChildren);
         continue;
      }
   }

   this->needsSort = false;

   // Check consistency of the built structures
   assert(Vector_size(this->displayList) == vsize); (void)vsize;
}

void ProcessList_updateDisplayList(ProcessList* this) {
   if (this->settings->ss->treeView) {
      if (this->needsSort)
         ProcessList_buildTree(this);
   } else {
      if (this->needsSort)
         Vector_insertionSort(this->processes);
      Vector_prune(this->displayList);
      int size = Vector_size(this->processes);
      for (int i = 0; i < size; i++)
         Vector_add(this->displayList, Vector_get(this->processes, i));
   }
   this->needsSort = false;
}

ProcessField ProcessList_keyAt(const ProcessList* this, int at) {
   int x = 0;
   const ProcessField* fields = this->settings->ss->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(alignedProcessFieldTitle(this, field));
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

// Called on collapse-all toggle and on startup, possibly in non-tree mode
void ProcessList_collapseAllBranches(ProcessList* this) {
   ProcessList_buildTree(this); // Update `tree_depth` fields of the processes
   this->needsSort = true; // ProcessList is sorted by parent now, force new sort
   int size = Vector_size(this->processes);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) Vector_get(this->processes, i);
      // FreeBSD has pid 0 = kernel and pid 1 = init, so init has tree_depth = 1
      if (process->tree_depth > 0 && process->pid > 1)
         process->showChildren = false;
   }
}

void ProcessList_rebuildPanel(ProcessList* this) {
   ProcessList_updateDisplayList(this);

   const char* incFilter = this->incFilter;

   const int currPos = Panel_getSelectedIndex(this->panel);
   const int currScrollV = this->panel->scrollV;
   const int currSize = Panel_size(this->panel);

   Panel_prune(this->panel);

   /* Follow main process if followed a userland thread and threads are now hidden */
   const Settings* settings = this->settings;
   if (this->following != -1 && settings->hideUserlandThreads) {
      const Process* followedProcess = (const Process*) Hashtable_get(this->processTable, this->following);
      if (followedProcess && Process_isThread(followedProcess) && Hashtable_get(this->processTable, followedProcess->tgid) != NULL) {
         this->following = followedProcess->tgid;
      }
   }

   const int processCount = Vector_size(this->displayList);
   int idx = 0;
   bool foundFollowed = false;

   for (int i = 0; i < processCount; i++) {
      Process* p = (Process*) Vector_get(this->displayList, i);

      if ( (!p->show)
         || (this->userId != (uid_t) -1 && (p->st_uid != this->userId))
         || (incFilter && !(String_contains_i(Process_getCommand(p), incFilter, true)))
         || (this->pidMatchList && !Hashtable_get(this->pidMatchList, p->tgid)) )
         continue;

      Panel_set(this->panel, idx, (Object*)p);

      if (this->following != -1 && p->pid == this->following) {
         foundFollowed = true;
         Panel_setSelected(this->panel, idx);
         this->panel->scrollV = currScrollV;
      }
      idx++;
   }

   if (this->following != -1 && !foundFollowed) {
      /* Reset if current followed pid not found */
      this->following = -1;
      Panel_setSelectionColor(this->panel, PANEL_SELECTION_FOCUS);
   }

   if (this->following == -1) {
      /* If the last item was selected, keep the new last item selected */
      if (currPos > 0 && currPos == currSize - 1)
         Panel_setSelected(this->panel, Panel_size(this->panel) - 1);
      else
         Panel_setSelected(this->panel, currPos);

      this->panel->scrollV = currScrollV;
   }
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor) {
   Process* proc = (Process*) Hashtable_get(this->processTable, pid);
   *preExisting = proc != NULL;
   if (proc) {
      assert(Vector_indexOf(this->processes, proc, Process_pidEqualCompare) != -1);
      assert(proc->pid == pid);
   } else {
      proc = constructor(this->settings);
      assert(proc->cmdline == NULL);
      proc->pid = pid;
   }
   return proc;
}

void ProcessList_scan(ProcessList* this, bool pauseProcessUpdate) {
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

   Process_resetFieldWidths();

   // set scan timestamp
   static bool firstScanDone = false;
   if (firstScanDone) {
      Platform_gettime_monotonic(&this->monotonicMs);
   } else {
      this->monotonicMs = 0;
      firstScanDone = true;
   }

   ProcessList_goThroughEntries(this, false);

   uid_t maxUid = 0;
   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      Process_makeCommandStr(p);

      // keep track of the highest UID for column scaling
      if (p->st_uid > maxUid)
         maxUid = p->st_uid;

      if (p->tombStampMs > 0) {
         // remove tombed process
         if (this->monotonicMs >= p->tombStampMs) {
            ProcessList_removeIndex(this, p, i);
         }
      } else if (p->updated == false) {
         // process no longer exists
         if (this->settings->highlightChanges && p->wasShown) {
            // mark tombed
            p->tombStampMs = this->monotonicMs + 1000 * this->settings->highlightDelaySecs;
         } else {
            // immediately remove
            ProcessList_removeIndex(this, p, i);
         }
      }
   }

   // Compact the processes vector in case of any deletions
   Vector_compact(this->processes);

   // Set UID column width based on max UID.
   Process_setUidColumnWidth(maxUid);
}
