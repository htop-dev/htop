/*
htop - PCPGenericDataList.c
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "pcp/PCPGenericDataList.h"

#include <stddef.h>
#include <stdlib.h>

#include "DynamicColumn.h"
#include "GenericData.h"
#include "GenericDataList.h"
#include "Hashtable.h"
#include "Object.h"
#include "Process.h"
#include "Settings.h"
#include "Vector.h"
#include "XUtils.h"

#include "pcp/PCPDynamicColumn.h"
#include "pcp/PCPGenericData.h"
#include "pcp/PCPMetric.h"


static const PCPDynamicColumn* defineKeyMetric(const Settings* settings) {
   const PCPDynamicColumn* column;
   column = (PCPDynamicColumn*)Hashtable_get(settings->dynamicColumns, settings->ss->fields[0]);
   if (!column)
      return NULL;

   return column;
}

static int getColumnCount(const ProcessField* fields) {
   int count = 0;
   for (unsigned int i = 0; fields[i]; i++)
      count = i;

   return count + 1;
}

static int getRowCount(int keyMetric) {
   int count = PCPMetric_instanceCount(keyMetric);
   return (count > 0) ? count : 0;
}

static void allocRows(GenericDataList* gl, int requiredRows) {
   GenericData* g;
   int currentRowsCount = Vector_size(gl->genericDataRow);

   if (currentRowsCount != requiredRows) {
      if (currentRowsCount > requiredRows) {
         for (int r = currentRowsCount; r > requiredRows; r--) {
            int idx = r - 1;

            PCPGenericData* gg = (PCPGenericData*)Vector_get(gl->genericDataRow, idx);
            PCPGenericData_removeAllFields(gg);
            GenericDataList_removeGenericData(gl);
         }
      }
      if (currentRowsCount < requiredRows) {
         for (int r = currentRowsCount; r < requiredRows; r++) {
            g = GenericDataList_getGenericData(gl, PCPGenericData_new);
            GenericDataList_addGenericData(gl, g);
         }
      }
   }
}

static void allocColumns(GenericDataList* gl, int requiredColumns, int requiredRows) {
   PCPGenericData* firstrow = (PCPGenericData*)Vector_get(gl->genericDataRow, 0);
   int currentColumns = firstrow->fieldsCount;

   if (currentColumns < requiredRows) {
      for (int r = 0; r < Vector_size(gl->genericDataRow); r++) {
         PCPGenericData* gg = (PCPGenericData*)Vector_get(gl->genericDataRow, r);
         for (int c = gg->fieldsCount; c < requiredColumns; c++)
            PCPGenericData_addField(gg);
      }
   }
   if (currentColumns > requiredRows) {
      for (int r = 0; r < Vector_size(gl->genericDataRow); r++) {
         PCPGenericData* gg = (PCPGenericData*)Vector_get(gl->genericDataRow, r);
         for (int c = gg->fieldsCount; c > requiredColumns; c--)
            PCPGenericData_removeField(gg);
      }
   }
}

static int PCPGenericDataList_updateGenericDataList(PCPGenericDataList* this) {
   GenericDataList* gl = (GenericDataList*) this;
   const Settings* settings = gl->settings;
   const ProcessField* fields = settings->ss->fields;
   const PCPDynamicColumn* keyMetric;

   keyMetric = defineKeyMetric(settings);

   if (!settings->ss->generic)
      return 0;

   // one or more columns are added to pcp/screens and do not exist in
   // pcp/columns, continue.
   for (unsigned int i = 0 ; !fields[i]; i++)
      return 0;

   // Instance Domain Validation
   // check if all columns share the same InDom
   pmInDom keyInDom = PCPMetric_InDom(keyMetric->id);
   for (unsigned int i = 0 ; fields[i]; i++) {
      PCPDynamicColumn* dc = Hashtable_get(settings->dynamicColumns, fields[i]);
      if (!dc)
         return -1;

      pmInDom indom = PCPMetric_InDom(dc->id);
      if (keyInDom != indom) {
         return 0;
      }
   }

   int requiredColumns = getColumnCount(fields);
   int requiredRows = getRowCount(keyMetric->id);

   // alloc
   allocRows(gl, requiredRows);
   if (requiredRows == 0)
      return 0;

   allocColumns(gl, requiredColumns, requiredRows);

   // fill
   int interInst = -1, offset = -1;
   while (PCPMetric_iterate(keyMetric->id, &interInst, &offset)) {
      for (unsigned int i = 0; fields[i]; i++) {
         int metricType;

         DynamicColumn* dc = Hashtable_get(settings->dynamicColumns, fields[i]);
         if (!dc)
            return -1;

         PCPDynamicColumn* column = (PCPDynamicColumn*) dc;
         metricType = PCPMetric_type(column->id);

         pmAtomValue value;
         if (PCPMetric_instance(column->id, interInst, offset, &value, metricType)) {
            GenericData* g;

            g = Hashtable_get(gl->genericDataTable, offset);
            if (!g)
               return -1;

            PCPGenericData* gg = (PCPGenericData*) g;

            PCPGenericDataField* field = (PCPGenericDataField*)Hashtable_get(gg->fields, i);
            if (!field)
               return -1;

            gg->offset       = offset;
            *field->value    = value;
            field->type      = metricType;
            field->pmid      = column->id;
            field->offset    = offset;
            field->interInst = interInst;
            field->units     = PCPMetric_units(column->id);
         }
      }
   }

   return 0;
}

void GenericDataList_goThroughEntries(GenericDataList* super, bool pauseUpdate)
{
   bool enabled = !pauseUpdate;
   PCPGenericDataList* this = (PCPGenericDataList*) super;

   if (enabled)
      PCPGenericDataList_updateGenericDataList(this);
}

GenericDataList* GenericDataList_addPlatformList(GenericDataList* super)
{
   PCPGenericDataList* this = xCalloc(1, sizeof(PCPGenericDataList));
   super = &(this->super);

   super->genericDataRow = Vector_new(Class(GenericData), false, DEFAULT_SIZE);
   super->displayList = Vector_new(Class(GenericData), false, DEFAULT_SIZE);
   super->genericDataTable = Hashtable_new(200, false);

   super->totalRows = 0;
   super->needsSort = true;

   return super;
}

void GenericDataList_removePlatformList(GenericDataList* gl)
{
   // SMA FIXME loop & GenericDataList_removeGenericData()
   Hashtable_delete(gl->genericDataTable);
   Vector_delete(gl->genericDataRow);

   PCPGenericDataList* this = (PCPGenericDataList*) gl;
   free(this);
}
