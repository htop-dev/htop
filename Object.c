/*
htop
(C) 2004-2006 Hisham H. Muhammad
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
typedef struct Object_ Object;

typedef void(*Object_Display)(Object*, RichString*);
typedef int(*Object_Compare)(const Object*, const Object*);
typedef void(*Object_Delete)(Object*);

struct Object_ {
   char* class;
   Object_Display display;
   Object_Compare compare;
   Object_Delete delete;
};
}*/

/* private property */
char* OBJECT_CLASS = "Object";

void Object_new() {
   Object* this;
   this = malloc(sizeof(Object));
   this->class = OBJECT_CLASS;
   this->display = Object_display;
   this->compare = Object_compare;
   this->delete = Object_delete;
}

bool Object_instanceOf(Object* this, char* class) {
   return this->class == class;
}

void Object_delete(Object* this) {
   free(this);
}

void Object_display(Object* this, RichString* out) {
   char objAddress[50];
   sprintf(objAddress, "%s @ %p", this->class, (void*) this);
   RichString_write(out, CRT_colors[DEFAULT_COLOR], objAddress);
}

int Object_compare(const Object* this, const Object* o) {
   return (this - o);
}
