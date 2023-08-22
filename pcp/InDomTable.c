/*
htop - InDomTable.c
(C) 2023 htop dev team
(C) 2022-2023 Sohaib Mohammed
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "InDomTable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Platform.h"
#include "Table.h"
#include "Vector.h"
#include "XUtils.h"

#include "pcp/Instance.h"
#include "pcp/PCPDynamicColumn.h"
#include "pcp/PCPMetric.h"


InDomTable* InDomTable_new(Machine* host, pmInDom indom, int metricKey) {
   InDomTable* this = xCalloc(1, sizeof(InDomTable));
   Object_setClass(this, Class(InDomTable));
   this->metricKey = metricKey;
   this->id = indom;

   Table* super = &this->super;
   Table_init(super, Class(Row), host);

   return this;
}

void InDomTable_done(InDomTable* this) {
   Table_done(&this->super);
}

static void InDomTable_delete(Object* cast) {
   InDomTable* this = (InDomTable*) cast;
   InDomTable_done(this);
   free(this);
}

static Instance* InDomTable_getInstance(InDomTable* this, int id, bool* preExisting) {
   const Table* super = &this->super;
   Instance* inst = (Instance*) Hashtable_get(super->table, id);
   *preExisting = inst != NULL;
   if (inst) {
      assert(Vector_indexOf(super->rows, inst, Row_idEqualCompare) != -1);
      assert(Instance_getId(inst) == id);
   } else {
      inst = Instance_new(super->host, this);
      assert(inst->name == NULL);
      Instance_setId(inst, id);
   }
   return inst;
}

static void InDomTable_goThroughEntries(InDomTable* this) {
   Table* super = &this->super;

   /* for every instance ... */
   int instid = -1, offset = -1;
   while (PCPMetric_iterate(this->metricKey, &instid, &offset)) {
      bool preExisting;
      Instance* inst = InDomTable_getInstance(this, instid, &preExisting);
      inst->offset = offset >= 0 ? offset : 0;

      Row* row = &inst->super;
      if (!preExisting)
         Table_add(super, row);
      row->updated = true;
      row->show = true;
   }
}

static void InDomTable_iterateEntries(Table* super) {
   InDomTable* this = (InDomTable*) super;
   InDomTable_goThroughEntries(this);
}

const TableClass InDomTable_class = {
   .super = {
      .extends = Class(Table),
      .delete = InDomTable_delete,
   },
   .prepare = Table_prepareEntries,
   .iterate = InDomTable_iterateEntries,
   .cleanup = Table_cleanupEntries,
};
