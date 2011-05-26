/*
htop
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"
#include "RichString.h"
#include "CRT.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "debug.h"

/*{

#ifndef DEBUG
#define Object_setClass(obj, class)
#endif

typedef struct Object_ Object;

typedef void(*Object_Display)(Object*, RichString*);
typedef int(*Object_Compare)(const void*, const void*);
typedef void(*Object_Delete)(Object*);

struct Object_ {
   #ifdef DEBUG
   char* class;
   #endif
   Object_Display display;
   Object_Delete delete;
};
}*/

#ifdef DEBUG
char* OBJECT_CLASS = "Object";

#else
#define OBJECT_CLASS NULL
#endif

#ifdef DEBUG

void Object_setClass(void* this, char* class) {
   ((Object*)this)->class = class;
}

static void Object_display(Object* this, RichString* out) {
   char objAddress[50];
   sprintf(objAddress, "%s @ %p", this->class, (void*) this);
   RichString_write(out, CRT_colors[DEFAULT_COLOR], objAddress);
}

#endif
