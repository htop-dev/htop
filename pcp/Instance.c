/*
htop - Instance.c
(C) 2022-2023 Sohaib Mohammed
(C) 2022-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/Instance.h"

#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "DynamicScreen.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Macros.h"
#include "PCPDynamicColumn.h"
#include "PCPDynamicScreen.h"
#include "PCPMetric.h"
#include "Platform.h"
#include "Row.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/InDomTable.h"
#include "pcp/PCPMetric.h"


Instance* Instance_new(const Machine* host, const InDomTable* indom) {
   Instance* this = xCalloc(1, sizeof(Instance));
   Object_setClass(this, Class(Instance));

   Row* super = &this->super;
   Row_init(super, host);

   this->indom = indom;

   return this;
}

void Instance_done(Instance* this) {
   if (this->name)
      free(this->name);
   Row_done(&this->super);
}

static void Instance_delete(Object* cast) {
   Instance* this = (Instance*) cast;
   Instance_done(this);
   free(this);
}

static void Instance_writeField(const Row* super, RichString* str, RowField field) {
   const Instance* this = (const Instance*) super;
   int instid = Instance_getId(this);

   const Settings* settings = super->host->settings;
   DynamicColumn* column = Hashtable_get(settings->dynamicColumns, field);
   PCPDynamicColumn* cp = (PCPDynamicColumn*) column;
   const pmDesc* descp = PCPMetric_desc(cp->id);

   pmAtomValue atom;
   pmAtomValue *ap = &atom;
   if (!PCPMetric_instance(cp->id, instid, this->offset, ap, descp->type))
      ap = NULL;

   PCPDynamicColumn_writeAtomValue(cp, str, settings, cp->id, instid, descp, ap);

   if (ap && descp->type == PM_TYPE_STRING)
      free(ap->cp);
}

static const char* Instance_externalName(Row* super) {
   Instance* this = (Instance*) super;

   if (!this->name)
      pmNameInDom(InDom_getId(this), Instance_getId(this), &this->name);
   return this->name;
}

static int Instance_compareByKey(const Row* v1, const Row* v2, int key) {
   const Instance* i1 = (const Instance*)v1;
   const Instance* i2 = (const Instance*)v2;

   if (key < 0)
      return 0;

   Hashtable* dc = Platform_dynamicColumns();
   const PCPDynamicColumn* column = Hashtable_get(dc, key);
   if (!column)
      return -1;

   size_t metric = column->id;
   unsigned int type = PCPMetric_type(metric);

   pmAtomValue atom1 = {0}, atom2 = {0};
   if (!PCPMetric_instance(metric, i1->offset, i1->offset, &atom1, type) ||
       !PCPMetric_instance(metric, i2->offset, i2->offset, &atom2, type)) {
      if (type == PM_TYPE_STRING) {
         free(atom1.cp);
         free(atom2.cp);
      }
      return -1;
   }

   switch (type) {
      case PM_TYPE_STRING: {
         int cmp = SPACESHIP_NULLSTR(atom2.cp, atom1.cp);
         free(atom2.cp);
         free(atom1.cp);
         return cmp;
      }
      case PM_TYPE_32:
         return SPACESHIP_NUMBER(atom2.l, atom1.l);
      case PM_TYPE_U32:
         return SPACESHIP_NUMBER(atom2.ul, atom1.ul);
      case PM_TYPE_64:
         return SPACESHIP_NUMBER(atom2.ll, atom1.ll);
      case PM_TYPE_U64:
         return SPACESHIP_NUMBER(atom2.ull, atom1.ull);
      case PM_TYPE_FLOAT:
         return SPACESHIP_NUMBER(atom2.f, atom1.f);
      case PM_TYPE_DOUBLE:
         return SPACESHIP_NUMBER(atom2.d, atom1.d);
      default:
         break;
   }

   return 0;
}

static int Instance_compare(const void* v1, const void* v2) {
   const Instance* i1 = (const Instance*)v1;
   const Instance* i2 = (const Instance*)v2;
   const ScreenSettings* ss = i1->super.host->settings->ss;
   RowField key = ScreenSettings_getActiveSortKey(ss);
   int result = Instance_compareByKey(v1, v2, key);

   // Implement tie-breaker (needed to make tree mode more stable)
   if (!result)
      return SPACESHIP_NUMBER(Instance_getId(i1), Instance_getId(i2));

   return (ScreenSettings_getActiveDirection(ss) == 1) ? result : -result;
}

const RowClass Instance_class = {
   .super = {
      .extends = Class(Row),
      .display = Row_display,
      .delete = Instance_delete,
      .compare = Instance_compare,
   },
   .sortKeyString = Instance_externalName,
   .writeField = Instance_writeField,
};
