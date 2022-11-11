#ifndef HEADER_PCPGenericData
#define HEADER_PCPGenericData
/*
htop - PCPGenericData.h
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "GenericData.h"
#include "Hashtable.h"
#include "Object.h"
#include "Platform.h"
#include "Settings.h"


typedef struct PCPGenericDataField_ {
   pmAtomValue value;
   size_t id;  /* identifier for metric array lookups */
   int offset;
   int instance;
   const pmDesc* desc;
} PCPGenericDataField;

typedef struct PCPGenericData_ {
   GenericData super;
   Hashtable* fields;  /* PCPGenericDataFields */

   /* default result offset to use for searching metrics with instances */
   unsigned int offset;
   size_t fieldsCount;
   int sortKey;
} PCPGenericData;


void PCPGenericData_delete(Object* cast);

int PCPGenericData_compare(const void* v1, const void* v2);

extern const GenericDataClass PCPGenericData_class;

GenericData* PCPGenericData_new(const Settings* settings);

void GenericData_delete(Object* cast);

PCPGenericDataField* PCPGenericData_addField(PCPGenericData* this);

void PCPGenericData_removeField(PCPGenericData* this);

void PCPGenericData_removeAllFields(PCPGenericData* this);

#endif
