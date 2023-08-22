/*
htop - Machine.c
(C) 2023 Red Hat, Inc.
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"

#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Platform.h"
#include "Row.h"
#include "XUtils.h"


void Machine_init(Machine* this, UsersTable* usersTable, uid_t userId) {
   this->usersTable = usersTable;
   this->userId = userId;

   this->htopUserId = getuid();

   // discover fixed column width limits
   Row_setPidColumnWidth(Platform_getMaxPid());

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
}

void Machine_done(Machine* this) {
#ifdef HAVE_LIBHWLOC
   if (this->topologyOk) {
      hwloc_topology_destroy(this->topology);
   }
#endif
   for (size_t i = 0; i < this->tableCount; i++) {
      Object_delete(&this->tables[i]->super);
   }
}

void Machine_addTable(Machine* this, Table* table, bool processes) {
   if (processes)
      this->processTable = table;
   this->activeTable = table;

   size_t nmemb = this->tableCount + 1;
   Table** tables = xReallocArray(this->tables, nmemb, sizeof(Table*));
   tables[nmemb - 1] = table;
   this->tables = tables;
   this->tableCount++;
}

void Machine_scanTables(Machine* this) {
   // set scan timestamp
   static bool firstScanDone = false;

   if (firstScanDone)
      Platform_gettime_monotonic(&this->monotonicMs);
   else
      firstScanDone = true;

   this->maxUserId = 0;
   Row_resetFieldWidths();

   for (size_t i = 0; i < this->tableCount; i++) {
      Table* table = this->tables[i];

      // pre-processing of each row
      Table_scanPrepare(table);

      // scan values for this table
      Table_scanIterate(table);

      // post-process after scanning
      Table_scanCleanup(table);
   }

   Row_setUidColumnWidth(this->maxUserId);
}
