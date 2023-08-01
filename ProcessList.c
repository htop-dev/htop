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


void ProcessList_init(ProcessList* this, const ObjectClass* klass, Machine* host, Hashtable* pidMatchList) {
   this->processes = Vector_new(klass, true, DEFAULT_SIZE);
   this->displayList = Vector_new(klass, false, DEFAULT_SIZE);
   this->processTable = Hashtable_new(200, false);
   this->pidMatchList = pidMatchList;
   this->needsSort = true;
   this->following = -1;
   this->host = host;
}

void ProcessList_done(ProcessList* this) {
   Hashtable_delete(this->processTable);
   Vector_delete(this->displayList);
   Vector_delete(this->processes);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

// helper function to fill an aligned title string for a dynamic column
static const char* alignedTitleDynamicColumn(const Settings* settings, int key, char* titleBuffer, size_t titleBufferSize) {
   const DynamicColumn* column = Hashtable_get(settings->dynamicColumns, key);
   if (column == NULL)
      return "- ";
   int width = column->width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   xSnprintf(titleBuffer, titleBufferSize, "%*s", width, column->heading);
   return titleBuffer;
}

// helper function to fill an aligned title string for a process field
static const char* alignedTitleProcessField(ProcessField field, char* titleBuffer, size_t titleBufferSize) {
   const char* title = Process_fields[field].title;
   if (!title)
      return "- ";

   if (Process_fields[field].pidColumn) {
      xSnprintf(titleBuffer, titleBufferSize, "%*s ", Process_pidDigits, title);
      return titleBuffer;
   }

   if (field == ST_UID) {
      xSnprintf(titleBuffer, titleBufferSize, "%*s ", Process_uidDigits, title);
      return titleBuffer;
   }

   if (Process_fields[field].autoWidth) {
      if (field == PERCENT_CPU)
         xSnprintf(titleBuffer, titleBufferSize, "%*s ", Process_fieldWidths[field], title);
      else
         xSnprintf(titleBuffer, titleBufferSize, "%-*.*s ", Process_fieldWidths[field], Process_fieldWidths[field], title);
      return titleBuffer;
   }

   return title;
}

// helper function to create an aligned title string for a given field
static const char* ProcessField_alignedTitle(const Settings* settings, ProcessField field) {
   static char titleBuffer[UINT8_MAX + sizeof(" ")];
   assert(sizeof(titleBuffer) >= DYNAMIC_MAX_COLUMN_WIDTH + sizeof(" "));
   assert(sizeof(titleBuffer) >= PROCESS_MAX_PID_DIGITS + sizeof(" "));
   assert(sizeof(titleBuffer) >= PROCESS_MAX_UID_DIGITS + sizeof(" "));

   if (field < LAST_PROCESSFIELD)
      return alignedTitleProcessField(field, titleBuffer, sizeof(titleBuffer));
   return alignedTitleDynamicColumn(settings, field, titleBuffer, sizeof(titleBuffer));
}

void ProcessList_printHeader(const ProcessList* this, RichString* header) {
   RichString_rewind(header, RichString_size(header));

   const Settings* settings = this->host->settings;
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

      RichString_appendWide(header, color, ProcessField_alignedTitle(settings, fields[i]));
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

   // highlighting processes found in first scan by first scan marked "far in the past"
   p->seenStampMs = this->host->monotonicMs;

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

static void ProcessList_buildTreeBranch(ProcessList* this, pid_t pid, unsigned int level, int32_t indent, bool show) {
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

      int32_t nextIndent = indent | ((int32_t)1 << MINIMUM(level, sizeof(process->indent) * 8 - 2));
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
      process->tree_depth = 0;

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
         Vector_add(this->displayList, process);
         ProcessList_buildTreeBranch(this, process->pid, 0, 0, process->showChildren);
         continue;
      }
   }

   // Under some ptrace(2) implementations, a process ptrace(2)-attaching a
   // parent process in its process tree will causing the traced process to
   // be re-parented to the tracing process; this will creating a loop in the
   // process tree. In this case build separated tree(s) for loop(s) left
   // here.
   int display_size = Vector_size(this->displayList);
   if(display_size < vsize) {
      for (int i = 0; i < vsize; i++) {
         Process* process = (Process*)Vector_get(this->processes, i);
         if(!process->isRoot && process->tree_depth == 0) {
            // This lost process could either be a node at the loop itself,
            // or a descendant node indirectly attached to the loop.
            do {
               // Make sure we break at the loop itself, not a descendant of it.
               process->tree_depth = 1;
               Process *parent = ProcessList_findProcess(this, Process_getParentPid(process));
               assert(parent != NULL);
               if(!parent) break;
               process = parent;
               assert(!process->isRoot);
            } while(!process->isRoot && !process->tree_depth);
            process->isRoot = true;
            process->tree_depth = 0;
            process->indent = 0;
            Vector_add(this->displayList, process);
            ProcessList_buildTreeBranch(this, process->pid, 0, 0, process->showChildren);
            if(Vector_size(this->displayList) == vsize) break; // No more loop exist.
         }
         // There appears to have more loop(s) left, trying to continue find.
      }
   }

   this->needsSort = false;

   // Check consistency of the built structures
   assert(Vector_size(this->displayList) == vsize);
}

void ProcessList_updateDisplayList(ProcessList* this) {
   if (this->host->settings->ss->treeView) {
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
   const Settings* settings = this->host->settings;
   const ProcessField* fields = settings->ss->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(ProcessField_alignedTitle(settings, field));
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
   const Machine* host= this->host;
   const Settings* settings = host->settings;
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
         || (host->userId != (uid_t) -1 && (p->st_uid != host->userId))
         || (incFilter && !(String_contains_i(Process_getCommand(p), incFilter, true)))
         || (this->pidMatchList && !Hashtable_get(this->pidMatchList, p->tgid)) )
         continue;

      Panel_set(this->panel, idx, (Object*)p);

      if (this->following != -1 && p->pid == this->following) {
         foundFollowed = true;
         Panel_setSelected(this->panel, idx);
         /* Keep scroll position relative to followed process */
         this->panel->scrollV = idx - (currPos-currScrollV);
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
      proc = constructor(this->host);
      assert(proc->cmdline == NULL);
      proc->pid = pid;
   }
   return proc;
}

void ProcessList_scan(ProcessList* this) {
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
   Machine* host = this->host;
   if (firstScanDone) {
      Platform_gettime_monotonic(&host->monotonicMs);
   } else {
      host->monotonicMs = 0;
      firstScanDone = true;
   }

   ProcessList_goThroughEntries(this);

   uid_t maxUid = 0;
   const Settings* settings = host->settings;
   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      Process_makeCommandStr(p);

      // keep track of the highest UID for column scaling
      if (p->st_uid > maxUid)
         maxUid = p->st_uid;

      if (p->tombStampMs > 0) {
         // remove tombed process
         if (host->monotonicMs >= p->tombStampMs) {
            ProcessList_removeIndex(this, p, i);
         }
      } else if (p->updated == false) {
         // process no longer exists
         if (settings->highlightChanges && p->wasShown) {
            // mark tombed
            p->tombStampMs = host->monotonicMs + 1000 * settings->highlightDelaySecs;
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
