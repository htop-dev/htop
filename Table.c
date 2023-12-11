/*
htop - Table.c
(C) 2004,2005 Hisham H. Muhammad
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Table.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "CRT.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Macros.h"
#include "Panel.h"
#include "RowField.h"
#include "Vector.h"


Table* Table_init(Table* this, const ObjectClass* klass, Machine* host) {
   this->rows = Vector_new(klass, true, DEFAULT_SIZE);
   this->displayList = Vector_new(klass, false, DEFAULT_SIZE);
   this->table = Hashtable_new(200, false);
   this->needsSort = true;
   this->following = -1;
   this->host = host;
   return this;
}

void Table_done(Table* this) {
   Hashtable_delete(this->table);
   Vector_delete(this->displayList);
   Vector_delete(this->rows);
}

static void Table_delete(Object* cast) {
   Table* this = (Table*) cast;
   Table_done(this);
   free(this);
}

void Table_setPanel(Table* this, Panel* panel) {
   this->panel = panel;
}

void Table_add(Table* this, Row* row) {
   assert(Vector_indexOf(this->rows, row, Row_idEqualCompare) == -1);
   assert(Hashtable_get(this->table, row->id) == NULL);

   // highlighting row found in first scan by first scan marked "far in the past"
   row->seenStampMs = this->host->monotonicMs;

   Vector_add(this->rows, row);
   Hashtable_put(this->table, row->id, row);

   assert(Vector_indexOf(this->rows, row, Row_idEqualCompare) != -1);
   assert(Hashtable_get(this->table, row->id) != NULL);
   assert(Vector_countEquals(this->rows, Hashtable_count(this->table)));
}

// Table_removeIndex removes a given row from the lists map and soft deletes
// it from its vector. Vector_compact *must* be called once the caller is done
// removing items.
// Note: for processes should only be called from ProcessTable_iterate to avoid
// breaking dying process highlighting.
void Table_removeIndex(Table* this, const Row* row, int idx) {
   int rowid = row->id;

   assert(row == (Row*)Vector_get(this->rows, idx));
   assert(Hashtable_get(this->table, rowid) != NULL);

   Hashtable_remove(this->table, rowid);
   Vector_softRemove(this->rows, idx);

   if (this->following != -1 && this->following == rowid) {
      this->following = -1;
      Panel_setSelectionColor(this->panel, PANEL_SELECTION_FOCUS);
   }

   assert(Hashtable_get(this->table, rowid) == NULL);
   assert(Vector_countEquals(this->rows, Hashtable_count(this->table)));
}

static void Table_buildTreeBranch(Table* this, int rowid, unsigned int level, int32_t indent, bool show) {
   // Do not treat zero as root of any tree.
   // (e.g. on OpenBSD the kernel thread 'swapper' has pid 0.)
   if (rowid == 0)
      return;

   // The vector is sorted by parent, find the start of the range by bisection
   int vsize = Vector_size(this->rows);
   int l = 0;
   int r = vsize;
   while (l < r) {
      int c = (l + r) / 2;
      Row* row = (Row*)Vector_get(this->rows, c);
      int parent = row->isRoot ? 0 : Row_getGroupOrParent(row);
      if (parent < rowid) {
         l = c + 1;
      } else {
         r = c;
      }
   }
   // Find the end to know the last line for indent handling purposes
   int lastShown = r;
   while (r < vsize) {
      Row* row = (Row*)Vector_get(this->rows, r);
      if (!Row_isChildOf(row, rowid))
         break;
      if (row->show)
         lastShown = r;
      r++;
   }

   for (int i = l; i < r; i++) {
      Row* row = (Row*)Vector_get(this->rows, i);

      if (!show)
         row->show = false;

      Vector_add(this->displayList, row);

      int32_t nextIndent = indent | ((int32_t)1 << MINIMUM(level, sizeof(row->indent) * 8 - 2));
      Table_buildTreeBranch(this, row->id, level + 1, (i < lastShown) ? nextIndent : indent, row->show && row->showChildren);
      if (i == lastShown)
         row->indent = -nextIndent;
      else
         row->indent = nextIndent;

      row->tree_depth = level + 1;
   }
}

static int compareRowByKnownParentThenNatural(const void* v1, const void* v2) {
   return Row_compareByParent((const Row*) v1, (const Row*) v2);
}

// Builds a sorted tree from scratch, without relying on previously gathered information
static void Table_buildTree(Table* this) {
   Vector_prune(this->displayList);

   // Mark root processes
   int vsize = Vector_size(this->rows);
   for (int i = 0; i < vsize; i++) {
      Row* row = (Row*) Vector_get(this->rows, i);
      int parent = Row_getGroupOrParent(row);
      row->isRoot = false;

      if (row->id == parent) {
         row->isRoot = true;
         continue;
      }

      if (!parent) {
         row->isRoot = true;
         continue;
      }

      // We don't know about its parent for whatever reason
      if (Table_findRow(this, parent) == NULL)
         row->isRoot = true;
   }

   // Sort by known parent (roots first), then row ID
   Vector_quickSortCustomCompare(this->rows, compareRowByKnownParentThenNatural);

   // Find all processes whose parent is not visible
   for (int i = 0; i < vsize; i++) {
      Row* row = (Row*)Vector_get(this->rows, i);

      // If parent not found, then construct the tree with this node as root
      if (row->isRoot) {
         row = (Row*)Vector_get(this->rows, i);
         row->indent = 0;
         row->tree_depth = 0;
         Vector_add(this->displayList, row);
         Table_buildTreeBranch(this, row->id, 0, 0, row->showChildren);
         continue;
      }
   }

   this->needsSort = false;

   // Check consistency of the built structures
   assert(Vector_size(this->displayList) == vsize); (void)vsize;
}

void Table_updateDisplayList(Table* this) {
   const Settings* settings = this->host->settings;

   if (settings->ss->treeView) {
      if (this->needsSort)
         Table_buildTree(this);
   } else {
      if (this->needsSort)
         Vector_insertionSort(this->rows);
      Vector_prune(this->displayList);
      int size = Vector_size(this->rows);
      for (int i = 0; i < size; i++)
         Vector_add(this->displayList, Vector_get(this->rows, i));
   }
   this->needsSort = false;
}

void Table_expandTree(Table* this) {
   int size = Vector_size(this->rows);
   for (int i = 0; i < size; i++) {
      Row* row = (Row*) Vector_get(this->rows, i);
      row->showChildren = true;
   }
}

// Called on collapse-all toggle and on startup, possibly in non-tree mode
void Table_collapseAllBranches(Table* this) {
   Table_buildTree(this); // Update `tree_depth` fields of the rows
   this->needsSort = true; // Table is sorted by parent now, force new sort
   int size = Vector_size(this->rows);
   for (int i = 0; i < size; i++) {
      Row* row = (Row*) Vector_get(this->rows, i);
      // FreeBSD has pid 0 = kernel and pid 1 = init, so init has tree_depth = 1
      if (row->tree_depth > 0 && row->id > 1)
         row->showChildren = false;
   }
}

void Table_rebuildPanel(Table* this) {
   Table_updateDisplayList(this);

   const int currPos = Panel_getSelectedIndex(this->panel);
   const int currScrollV = this->panel->scrollV;
   const int currSize = Panel_size(this->panel);

   Panel_prune(this->panel);

   /* Follow main group row instead if following a row that is occluded (hidden) */
   if (this->following != -1) {
      const Row* followed = (const Row*) Hashtable_get(this->table, this->following);
      if (followed != NULL
         && Hashtable_get(this->table, followed->group)
         && Row_isVisible(followed, this) == false ) {
         this->following = followed->group;
      }
   }

   const int rowCount = Vector_size(this->displayList);
   bool foundFollowed = false;
   int idx = 0;

   for (int i = 0; i < rowCount; i++) {
      Row* row = (Row*) Vector_get(this->displayList, i);

      if ( !row->show || (Row_matchesFilter(row, this) == true) )
         continue;

      Panel_set(this->panel, idx, (Object*)row);

      if (this->following != -1 && row->id == this->following) {
         foundFollowed = true;
         Panel_setSelected(this->panel, idx);
         /* Keep scroll position relative to followed row */
         this->panel->scrollV = idx - (currPos - currScrollV);
      }
      idx++;
   }

   if (this->following != -1 && !foundFollowed) {
      /* Reset if current followed row not found */
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

void Table_printHeader(const Settings* settings, RichString* header) {
   RichString_rewind(header, RichString_size(header));

   const ScreenSettings* ss = settings->ss;
   const RowField* fields = ss->fields;

   RowField key = ScreenSettings_getActiveSortKey(ss);

   for (int i = 0; fields[i]; i++) {
      int color;
      if (ss->treeView && ss->treeViewAlwaysByPID) {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      } else if (key == fields[i]) {
         color = CRT_colors[PANEL_SELECTION_FOCUS];
      } else {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      }

      RichString_appendWide(header, color, RowField_alignedTitle(settings, fields[i]));
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

// set flags on an existing rows before refreshing table
void Table_prepareEntries(Table* this) {
   for (int i = 0; i < Vector_size(this->rows); i++) {
      Row* row = (struct Row_*) Vector_get(this->rows, i);
      row->updated = false;
      row->wasShown = row->show;
      row->show = true;
   }
}

// tidy up Row state after refreshing the table
void Table_cleanupRow(Table* table, Row* row, int idx) {
   Machine* host = table->host;
   const Settings* settings = host->settings;

   if (row->tombStampMs > 0) {
      // remove tombed process
      if (host->monotonicMs >= row->tombStampMs) {
         Table_removeIndex(table, row, idx);
      }
   } else if (row->updated == false) {
      // process no longer exists
      if (settings->highlightChanges && row->wasShown) {
         // mark tombed
         row->tombStampMs = host->monotonicMs + 1000 * settings->highlightDelaySecs;
      } else {
         // immediately remove
         Table_removeIndex(table, row, idx);
      }
   }
}

void Table_cleanupEntries(Table* this) {
   // Finish process table update, culling any removed rows
   for (int i = Vector_size(this->rows) - 1; i >= 0; i--) {
      Row* row = (Row*) Vector_get(this->rows, i);
      Table_cleanupRow(this, row, i);
   }

   // compact the table in case of any earlier row removals
   Table_compact(this);
}

const TableClass Table_class = {
   .super = {
      .extends = Class(Object),
      .delete = Table_delete,
   },
   .prepare = Table_prepareEntries,
   .cleanup = Table_cleanupEntries,
};
