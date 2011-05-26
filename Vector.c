/*
htop
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"
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

typedef struct Vector_ {
   Object **array;
   Object_Compare compare;
   int arraySize;
   int growthRate;
   int items;
   char* vectorType;
   bool owner;
} Vector;

}*/

Vector* Vector_new(char* vectorType_, bool owner, int size, Object_Compare compare) {
   Vector* this;

   if (size == DEFAULT_SIZE)
      size = 10;
   this = (Vector*) malloc(sizeof(Vector));
   this->growthRate = size;
   this->array = (Object**) calloc(size, sizeof(Object*));
   this->arraySize = size;
   this->items = 0;
   this->vectorType = vectorType_;
   this->owner = owner;
   this->compare = compare;
   return this;
}

void Vector_delete(Vector* this) {
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i])
            (this->array[i])->delete(this->array[i]);
   }
   free(this->array);
   free(this);
}

#ifdef DEBUG

static inline bool Vector_isConsistent(Vector* this) {
   assert(this->items <= this->arraySize);
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i] && this->array[i]->class != this->vectorType)
            return false;
      return true;
   } else {
      return true;
   }
}

int Vector_count(Vector* this) {
   int items = 0;
   for (int i = 0; i < this->items; i++) {
      if (this->array[i])
         items++;
   }
   assert(items == this->items);
   return items;
}

#endif

void Vector_prune(Vector* this) {
   assert(Vector_isConsistent(this));
   int i;

   for (i = 0; i < this->items; i++)
      if (this->array[i]) {
         if (this->owner)
            (this->array[i])->delete(this->array[i]);
         this->array[i] = NULL;
      }
   this->items = 0;
}

void Vector_sort(Vector* this) {
   assert(this->compare);
   assert(Vector_isConsistent(this));
   Object_Compare compare = this->compare;
   /* Insertion sort works best on mostly-sorted arrays. */
   for (int i = 1; i < this->items; i++) {
      int j;
      void* t = this->array[i];
      for (j = i-1; j >= 0 && compare(this->array[j], t) > 0; j--)
         this->array[j+1] = this->array[j];
      this->array[j+1] = t;
   }
   assert(Vector_isConsistent(this));
}

static void Vector_checkArraySize(Vector* this) {
   assert(Vector_isConsistent(this));
   if (this->items >= this->arraySize) {
      int i;
      i = this->arraySize;
      this->arraySize = this->items + this->growthRate;
      this->array = (Object**) realloc(this->array, sizeof(Object*) * this->arraySize);
      for (; i < this->arraySize; i++)
         this->array[i] = NULL;
   }
   assert(Vector_isConsistent(this));
}

void Vector_insert(Vector* this, int idx, void* data_) {
   assert(idx >= 0);
   assert(((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(Vector_isConsistent(this));
   
   Vector_checkArraySize(this);
   assert(this->array[this->items] == NULL);
   for (int i = this->items; i > idx; i--) {
      this->array[i] = this->array[i-1];
   }
   this->array[idx] = data;
   this->items++;
   assert(Vector_isConsistent(this));
}

Object* Vector_take(Vector* this, int idx) {
   assert(idx >= 0 && idx < this->items);
   assert(Vector_isConsistent(this));
   Object* removed = this->array[idx];
   assert (removed != NULL);
   this->items--;
   for (int i = idx; i < this->items; i++)
      this->array[i] = this->array[i+1];
   this->array[this->items] = NULL;
   assert(Vector_isConsistent(this));
   return removed;
}

Object* Vector_remove(Vector* this, int idx) {
   Object* removed = Vector_take(this, idx);
   if (this->owner) {
      removed->delete(removed);
      return NULL;
   } else
      return removed;
}

void Vector_moveUp(Vector* this, int idx) {
   assert(idx >= 0 && idx < this->items);
   assert(Vector_isConsistent(this));
   if (idx == 0)
      return;
   Object* temp = this->array[idx];
   this->array[idx] = this->array[idx - 1];
   this->array[idx - 1] = temp;
}

void Vector_moveDown(Vector* this, int idx) {
   assert(idx >= 0 && idx < this->items);
   assert(Vector_isConsistent(this));
   if (idx == this->items - 1)
      return;
   Object* temp = this->array[idx];
   this->array[idx] = this->array[idx + 1];
   this->array[idx + 1] = temp;
}

void Vector_set(Vector* this, int idx, void* data_) {
   assert(idx >= 0);
   assert(((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(Vector_isConsistent(this));

   Vector_checkArraySize(this);
   if (idx >= this->items) {
      this->items = idx+1;
   } else {
      if (this->owner) {
         Object* removed = this->array[idx];
         assert (removed != NULL);
         if (this->owner) {
            removed->delete(removed);
         }
      }
   }
   this->array[idx] = data;
   assert(Vector_isConsistent(this));
}

inline Object* Vector_get(Vector* this, int idx) {
   assert(idx < this->items);
   assert(Vector_isConsistent(this));
   return this->array[idx];
}

inline int Vector_size(Vector* this) {
   assert(Vector_isConsistent(this));
   return this->items;
}

/*

static void Vector_merge(Vector* this, Vector* v2) {
   int i;
   assert(Vector_isConsistent(this));
   
   for (i = 0; i < v2->items; i++)
      Vector_add(this, v2->array[i]);
   v2->items = 0;
   Vector_delete(v2);
   assert(Vector_isConsistent(this));
}

*/

void Vector_add(Vector* this, void* data_) {
   assert(data_ && ((Object*)data_)->class == this->vectorType);
   Object* data = data_;
   assert(Vector_isConsistent(this));
   int i = this->items;
   Vector_set(this, this->items, data);
   assert(this->items == i+1); (void)(i);
   assert(Vector_isConsistent(this));
}

inline int Vector_indexOf(Vector* this, void* search_, Object_Compare compare) {
   assert(((Object*)search_)->class == this->vectorType);
   assert(this->compare);
   Object* search = search_;
   assert(Vector_isConsistent(this));
   for (int i = 0; i < this->items; i++) {
      Object* o = (Object*)this->array[i];
      if (o && compare(search, o) == 0)
         return i;
   }
   return -1;
}
