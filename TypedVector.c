/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TypedVector.h"
#include "Object.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "debug.h"
#include <assert.h>

/*{

#ifndef DEFAULT_SIZE
#define DEFAULT_SIZE -1
#endif

typedef void(*TypedVector_procedure)(void*);

typedef struct TypedVector_ {
   Object **array;
   int arraySize;
   int growthRate;
   int items;
   char* vectorType;
   bool owner;
} TypedVector;

}*/

TypedVector* TypedVector_new(char* vectorType_, bool owner, int size) {
   TypedVector* this;

   if (size == DEFAULT_SIZE)
      size = 10;
   this = (TypedVector*) malloc(sizeof(TypedVector));
   this->growthRate = size;
   this->array = (Object**) calloc(size, sizeof(Object*));
   this->arraySize = size;
   this->items = 0;
   this->vectorType = vectorType_;
   this->owner = owner;
   return this;
}

void TypedVector_delete(TypedVector* this) {
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i])
            (this->array[i])->delete(this->array[i]);
   }
   free(this->array);
   free(this);
}

/* private */
bool TypedVector_isConsistent(TypedVector* this) {
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i] && this->array[i]->class != this->vectorType)
            return false;
      return true;
   } else {
      return true;
   }
}

void TypedVector_prune(TypedVector* this) {
   assert(TypedVector_isConsistent(this));
   int i;

   for (i = 0; i < this->items; i++)
      if (this->array[i]) {
         if (this->owner)
            (this->array[i])->delete(this->array[i]);
         this->array[i] = NULL;
      }
   this->items = 0;
}

void TypedVector_sort(TypedVector* this) {
   assert(TypedVector_isConsistent(this));
   int i, j;
   for (i = 1; i < this->items; i++) {
      void* t = this->array[i];
      for (j = i-1; j >= 0 && this->array[j]->compare(this->array[j], t) < 0; j--)
         this->array[j+1] = this->array[j];
      this->array[j+1] = t;
   }
   assert(TypedVector_isConsistent(this));

   /*
   for (int i = 0; i < this->items; i++) {
      for (int j = i+1; j < this->items; j++) {
         if (this->array[j]->compare(this->array[j], t) < 0) {
            void* tmp = this->array[i];
            this->array[i] = this->array[j];
            this->array[j] = tmp;
         }
      }
   }
   */
}

/* private */
void TypedVector_checkArraySize(TypedVector* this) {
   assert(TypedVector_isConsistent(this));
   if (this->items >= this->arraySize) {
      int i;
      i = this->arraySize;
      this->arraySize = this->items + this->growthRate;
      this->array = (Object**) realloc(this->array, sizeof(Object*) * this->arraySize);
      for (; i < this->arraySize; i++)
         this->array[i] = NULL;
   }
   assert(TypedVector_isConsistent(this));
}

void TypedVector_insert(TypedVector* this, int index, void* data_) {
   assert(index >= 0);
   assert(((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(TypedVector_isConsistent(this));
   
   TypedVector_checkArraySize(this);
   assert(this->array[this->items] == NULL);
   for (int i = this->items; i >= index; i--) {
      this->array[i+1] = this->array[i];
   }
   this->array[index] = data;
   this->items++;
   assert(TypedVector_isConsistent(this));
}

Object* TypedVector_take(TypedVector* this, int index) {
   assert(index >= 0 && index < this->items);
   assert(TypedVector_isConsistent(this));
   Object* removed = this->array[index];
   assert (removed != NULL);
   this->items--;
   for (int i = index; i < this->items; i++)
      this->array[i] = this->array[i+1];
   this->array[this->items] = NULL;
   assert(TypedVector_isConsistent(this));
   return removed;
}

Object* TypedVector_remove(TypedVector* this, int index) {
   Object* removed = TypedVector_take(this, index);
   if (this->owner) {
      removed->delete(removed);
      return NULL;
   } else
      return removed;
}

void TypedVector_moveUp(TypedVector* this, int index) {
   assert(index >= 0 && index < this->items);
   assert(TypedVector_isConsistent(this));
   if (index == 0)
      return;
   Object* temp = this->array[index];
   this->array[index] = this->array[index - 1];
   this->array[index - 1] = temp;
}

void TypedVector_moveDown(TypedVector* this, int index) {
   assert(index >= 0 && index < this->items);
   assert(TypedVector_isConsistent(this));
   if (index == this->items - 1)
      return;
   Object* temp = this->array[index];
   this->array[index] = this->array[index + 1];
   this->array[index + 1] = temp;
}

void TypedVector_set(TypedVector* this, int index, void* data_) {
   assert(index >= 0);
   assert(((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(TypedVector_isConsistent(this));

   TypedVector_checkArraySize(this);
   if (index >= this->items) {
      this->items = index+1;
   } else {
      if (this->owner) {
         Object* removed = this->array[index];
         assert (removed != NULL);
         if (this->owner) {
            removed->delete(removed);
         }
      }
   }
   this->array[index] = data;
   assert(TypedVector_isConsistent(this));
}

inline Object* TypedVector_get(TypedVector* this, int index) {
   assert(index < this->items);
   assert(TypedVector_isConsistent(this));
   return this->array[index];
}

inline int TypedVector_size(TypedVector* this) {
   assert(TypedVector_isConsistent(this));
   return this->items;
}

void TypedVector_merge(TypedVector* this, TypedVector* v2) {
   int i;
   assert(TypedVector_isConsistent(this));
   
   for (i = 0; i < v2->items; i++)
      TypedVector_add(this, v2->array[i]);
   v2->items = 0;
   TypedVector_delete(v2);
   assert(TypedVector_isConsistent(this));
}

void TypedVector_add(TypedVector* this, void* data_) {
   assert(data_ && ((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(TypedVector_isConsistent(this));

   TypedVector_set(this, this->items, data);
   assert(TypedVector_isConsistent(this));
}

inline int TypedVector_indexOf(TypedVector* this, void* search_) {
   assert(((Object*)search_)->class == this->vectorType);
   Object* search = search_;
   assert(TypedVector_isConsistent(this));

   int i;

   for (i = 0; i < this->items; i++) {
      Object* o = (Object*)this->array[i];
      if (o && o->compare(o, search) == 0)
         return i;
   }
   return -1;
}

void TypedVector_foreach(TypedVector* this, TypedVector_procedure f) {
   int i;
   assert(TypedVector_isConsistent(this));

   for (i = 0; i < this->items; i++)
      f(this->array[i]);
   assert(TypedVector_isConsistent(this));
}
