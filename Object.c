/*
htop - Object.c
(C) 2004-2012 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"

const ObjectClass Object_class = {
   .extends = NULL
};

#ifndef NDEBUG

bool Object_isA(const Object* o, const ObjectClass* klass) {
   if (!o)
      return false;
   const ObjectClass* type = o->klass;
   while (type) {
      if (type == klass)
         return true;
      type = type->extends;
   }
   return false;
}

#endif /* NDEBUG */
