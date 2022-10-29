/*
htop - GenericData.c
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPGenericData.h"

#include <stdbool.h>
#include <stdlib.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "DynamicScreen.h"
#include "GenericData.h"
#include "Hashtable.h"
#include "Macros.h"
#include "PCPDynamicColumn.h"
#include "PCPDynamicScreen.h"
#include "PCPMetric.h"
#include "Platform.h"
#include "Process.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/PCPMetric.h"


GenericData* PCPGenericData_new(const Settings* settings) {
   PCPGenericData* this = xCalloc(1, sizeof(PCPGenericData));
   Object_setClass(this, Class(PCPGenericData));

   this->fields = Hashtable_new(0, false);

   this->fieldsCount = 0;

   GenericData_init(&this->super, settings);
   return &this->super;
}

void GenericData_delete(Object* cast) {
   PCPGenericData* this = (PCPGenericData*) cast;

   for (size_t i = 1; i <= this->fieldsCount; i++)
      PCPGenericData_removeField(this);

   GenericData_done((GenericData*)cast);
   free(this);
}

PCPGenericDataField* PCPGenericData_addField(PCPGenericData* this)
{
   PCPGenericDataField* field = xCalloc(1, sizeof(PCPGenericDataField));
   pmAtomValue* atom = xCalloc(1, sizeof(pmAtomValue));

   field->value = atom;
   Hashtable_put(this->fields, this->fieldsCount, field);

   this->fieldsCount++;

   return field;
}

void PCPGenericData_removeField(PCPGenericData* this)
{
   int idx = this->fieldsCount - 1;

   PCPGenericDataField* field = Hashtable_get(this->fields, idx);
   free(field->value);
   Hashtable_remove(this->fields, idx);
   this->fieldsCount--;
}

void PCPGenericData_removeAllFields(PCPGenericData* this)
{
   for (size_t i = this->fieldsCount; i > 0; i--) {
      PCPGenericData_removeField(this);
   }
}

#define PrintField(format, value) \
   xSnprintf(buffer, sizeof(buffer), format, width, value);\
   RichString_appendAscii(str, attr, buffer);

#define KBytesField(format, val) \
   pmConvScale(format, gf->value, &gf->units, &atom, &kbyte_scale);\
   Process_printKBytes(str, val, coloring);

#define TimeField(format, val) \
   pmConvScale(format, gf->value, &gf->units, &atom, &sec_scale);\
   Process_printTime(str, val, coloring);

#define CountField(format, val) \
   pmConvScale(format, gf->value, &gf->units, &atom, &count_scale);\
   Process_printCount(str, val, coloring);

static void PCPGenericData_writeField(const GenericData* this, RichString* str, int field) {
   const PCPGenericData* gg = (const PCPGenericData*) this;
   PCPGenericDataField* gf = (PCPGenericDataField*)Hashtable_get(gg->fields, field);
   if (!gf)
      return;

   const ProcessField* fields = this->settings->ss->fields;
   char buffer[256];
   int attr = CRT_colors[DEFAULT_COLOR];
   bool coloring = this->settings->highlightMegabytes;

   DynamicColumn* dc = Hashtable_get(this->settings->dynamicColumns, fields[field]);
   if (!dc || !dc->enabled)
      return;

   PCPDynamicColumn* column = (PCPDynamicColumn*) dc;
   bool instances = column->instances;

   bool scaled = column->scale;
   pmAtomValue atom;
   pmUnits kbyte_scale;

   kbyte_scale.dimSpace = 1;
   kbyte_scale.scaleSpace = PM_SPACE_KBYTE;

   pmUnits sec_scale;
   sec_scale.dimTime = 1;
   sec_scale.scaleTime = PM_TIME_SEC;

   pmUnits count_scale;
   count_scale.dimCount = 1;
   count_scale.scaleCount = PM_COUNT_ONE;

   int width = column->super.width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   int abswidth = abs(width);
   if (abswidth > DYNAMIC_MAX_COLUMN_WIDTH) {
      abswidth = DYNAMIC_MAX_COLUMN_WIDTH;
      width = -abswidth;
   }

   if (instances) {
      char* instName;
      attr = CRT_colors[DYNAMIC_GRAY];

      PCPMetric_externalName(gf->pmid, gf->interInst, &instName);

      xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, 250, instName);
      RichString_appendAscii(str, attr, buffer);
   } else {
      switch (gf->type) {
         case PM_TYPE_STRING:
            attr = CRT_colors[DYNAMIC_GREEN];
            xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, 250, gf->value->cp);
            RichString_appendAscii(str, attr, buffer);
            break;
         case PM_TYPE_32:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_32, atom.l);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_32, atom.l);
                  break;
               } else {
                  CountField(PM_TYPE_32, atom.l);
                  break;
               }
            } else {
               PrintField("%*d ", gf->value->l);
               break;
            }
         case PM_TYPE_U32:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_U32, atom.ul);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_U32, atom.ul);
                  break;
               } else {
                  CountField(PM_TYPE_U32, atom.ul);
                  break;
               }
            } else {
               PrintField("%*u ", gf->value->ul);
               break;
            }
         case PM_TYPE_64:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_64, atom.ll);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_64, atom.ll);
                  break;
               } else {
                  CountField(PM_TYPE_64, atom.ll);
                  break;
               }
            } else {
               PrintField("%*lld ", (long long)gf->value->ll);
               break;
            }
         case PM_TYPE_U64:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_U64, atom.ull);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_U64, atom.ull);
                  break;
               } else {
                  CountField(PM_TYPE_U64, atom.ull);
                  break;
               }
            } else {
               PrintField("%*llu ", (unsigned long long)gf->value->ull);
               break;
            }
         case PM_TYPE_FLOAT:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_FLOAT, atom.f);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_FLOAT, atom.f);
                  break;
               } else {
                  CountField(PM_TYPE_FLOAT, atom.f);
                  break;
               }
            } else {
               PrintField("%*.2f ", (double)gf->value->f);
               break;
            }
         case PM_TYPE_DOUBLE:
            if (scaled) {
               if (gf->units.dimSpace) {
                  KBytesField(PM_TYPE_DOUBLE, atom.d);
                  break;
               } else if (gf->units.dimTime) {
                  TimeField(PM_TYPE_DOUBLE, atom.d);
                  break;
               } else {
                  CountField(PM_TYPE_DOUBLE, atom.d);
                  break;
               }
            } else {
               PrintField("%*.2f ", gf->value->d);
               break;
            }
         default:
            attr = CRT_colors[DYNAMIC_RED];
            RichString_appendAscii(str, attr, "no data ");
            break;
      }
   }
}

static int PCPGenericData_compareByKey(const GenericData* v1, const GenericData* v2, int key) {
   const PCPGenericData* g1 = (const PCPGenericData*)v1;
   const PCPGenericData* g2 = (const PCPGenericData*)v2;

   if (key < 0)
      return 0;

   Hashtable* dc = Platform_dynamicColumns();
   const PCPDynamicColumn* column = Hashtable_get(dc, key);
   if (!column)
      return -1;

   size_t metric = column->id;
   unsigned int type = PCPMetric_type(metric);

   pmAtomValue atom1 = {0}, atom2 = {0};
   if (!PCPMetric_instance(metric, g1->offset, g1->offset, &atom1, type) ||
       !PCPMetric_instance(metric, g2->offset, g2->offset, &atom2, type)) {
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

const GenericDataClass PCPGenericData_class = {
   .super = {
      .extends = Class(GenericData),
      .display = GenericData_display,
      .compare = GenericData_compare,
   },
   .writeField = PCPGenericData_writeField,
   .compareByKey = PCPGenericData_compareByKey,
};
