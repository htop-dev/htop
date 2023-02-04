/*
htop - GenericDataList.h
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "GenericDataList.h"

#include "Object.h"


GenericDataList* GenericDataList_new(void) {
   GenericDataList* gl = xCalloc(1, sizeof(GenericDataList));

   gl = GenericDataList_addPlatformList(gl);

   return gl;
}

void GenericDataList_delete(GenericDataList* this) {
   GenericDataList_removePlatformList(this);
}

void GenericDataList_addGenericData(GenericDataList* this, GenericData* g) {
   Vector_add(this->genericDataRow, g);
   Hashtable_put(this->genericDataTable, this->totalRows, g);

   this->totalRows++;
}

void GenericDataList_removeGenericData(GenericDataList* this) {
   int idx = this->totalRows - 1;
   Object* last = Vector_get(this->genericDataRow, idx);

   GenericData_delete(last);

   Vector_remove(this->genericDataRow, idx);
   Hashtable_remove(this->genericDataTable, idx);

   this->totalRows--;
}

GenericData* GenericDataList_getGenericData(GenericDataList* this, GenericData_New constructor) {
   GenericData* g = constructor(this->settings);

   return g;
}

void GenericDataList_scan(GenericDataList* this, bool pauseUpdate) {
   GenericDataList_goThroughEntries(this, pauseUpdate);
}

void GenericDataList_setPanel(GenericDataList* this, Panel* panel) {
   this->panel = panel;
}

static void GenericDataList_updateDisplayList(GenericDataList* this) {
   if (this->needsSort)
      Vector_insertionSort(this->genericDataRow);
   Vector_prune(this->displayList);
   int size = Vector_size(this->genericDataRow);
   for (int i = 0; i < size; i++)
      Vector_add(this->displayList, Vector_get(this->genericDataRow, i));
   this->needsSort = false;
}

void GenericDataList_rebuildPanel(GenericDataList* this) {
   GenericDataList_updateDisplayList(this);
   const int genericDataCount = Vector_size(this->displayList);
   int idx = 0;

   for (int i = 0; i < genericDataCount; i++) {
      GenericData* g = (GenericData*) Vector_get(this->displayList, i);

      Panel_set(this->panel, idx, (Object*)g);

      idx++;
   }
}
