/*
htop - GenericData.c
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "GenericData.h"

#include <assert.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "RichString.h"
#include "Settings.h"


void GenericData_init(GenericData* this, const Settings* settings) {
   this->settings = settings;
}

void GenericData_done(ATTR_UNUSED GenericData* this) {
   assert (this != NULL);
}

void GenericData_writeField(ATTR_UNUSED const GenericData* this, ATTR_UNUSED RichString* str, ATTR_UNUSED int field) {
   return;
}

void GenericData_display(const Object* cast, RichString* out) {
   const GenericData* this = (const GenericData*) cast;
   const ProcessField* fields = this->settings->ss->fields;
   for (int i = 0; fields[i]; i++)
      As_GenericData(this)->writeField(this, out, i);
}

int GenericData_compare(const void* v1, const void* v2) {
   const GenericData* g1 = (const GenericData*)v1;
   const GenericData* g2 = (const GenericData*)v2;

   const Settings* settings = g1->settings;
   const ScreenSettings* ss = settings->ss;

   ProcessField key = ScreenSettings_getActiveSortKey(ss);

   int result = GenericData_compareByKey(g1, g2, key);

   return (ScreenSettings_getActiveDirection(ss) == 1) ? result : -result;
}

int GenericData_compareByKey_Base(const GenericData* g1, const GenericData* g2, ATTR_UNUSED ProcessField key) {
   // TODO
   (void) g1;
   (void) g2;

   return 0;
}

const GenericDataClass GenericData_class = {
   .super = {
      .extends = Class(Object),
      .display = GenericData_display,
      .compare = GenericData_compare,
   },
   .writeField = GenericData_writeField,
};
