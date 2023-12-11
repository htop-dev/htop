#ifndef HEADER_Object
#define HEADER_Object
/*
htop - Object.h
(C) 2004-2012 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <assert.h>
#include <stdbool.h>

#include "RichString.h"
#include "XUtils.h" // IWYU pragma: keep


struct Object_;
typedef struct Object_ Object;

typedef void(*Object_Display)(const Object*, RichString*);
typedef int(*Object_Compare)(const void*, const void*);
typedef void(*Object_Delete)(Object*);

#define Object_getClass(obj_)         ((const Object*)(obj_))->klass
#define Object_setClass(obj_, class_) (((Object*)(obj_))->klass = (const ObjectClass*) (class_))

#define Object_delete(obj_)           (assert(Object_getClass(obj_)->delete), Object_getClass(obj_)->delete((Object*)(obj_)))
#define Object_displayFn(obj_)        Object_getClass(obj_)->display
#define Object_display(obj_, str_)    (assert(Object_getClass(obj_)->display), Object_getClass(obj_)->display((const Object*)(obj_), str_))
#define Object_compare(obj_, other_)  (assert(Object_getClass(obj_)->compare), Object_getClass(obj_)->compare((const void*)(obj_), other_))

#define Class(class_)                 ((const ObjectClass*)(&(class_ ## _class)))

#define AllocThis(class_) (class_*)   xMalloc(sizeof(class_)); Object_setClass(this, Class(class_))

typedef struct ObjectClass_ {
   const void* const extends;
   const Object_Display display;
   const Object_Delete delete;
   const Object_Compare compare;
} ObjectClass;

struct Object_ {
   const ObjectClass* klass;
};

typedef union {
   int i;
   void* v;
} Arg;

extern const ObjectClass Object_class;

bool Object_isA(const Object* o, const ObjectClass* klass);

#endif
