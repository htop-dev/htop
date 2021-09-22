/*
htop - Object.c
(C) 2004-2012 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"

#include <stddef.h>


const ObjectClass Object_class = {
   .extends = NULL
};

bool Object_isA(const Object* o, const ObjectClass* klass) {
   if (!o)
      return false;

   for (const ObjectClass* type = o->klass; type; type = type->extends) {
      if (type == klass) {
         return true;
      }
   }

   return false;
}
