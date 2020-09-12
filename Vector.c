/*
htop - Vector.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


Vector* Vector_new(ObjectClass* type, bool owner, int size) {
   Vector* this;

   if (size == DEFAULT_SIZE)
      size = 10;
   this = xMalloc(sizeof(Vector));
   this->growthRate = size;
   this->array = (Object**) xCalloc(size, sizeof(Object*));
   this->arraySize = size;
   this->items = 0;
   this->type = type;
   this->owner = owner;
   return this;
}

void Vector_delete(Vector* this) {
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i])
            Object_delete(this->array[i]);
   }
   free(this->array);
   free(this);
}

#ifdef DEBUG

static inline bool Vector_isConsistent(Vector* this) {
   assert(this->items <= this->arraySize);
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i] && !Object_isA(this->array[i], this->type))
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
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i]) {
            Object_delete(this->array[i]);
            //this->array[i] = NULL;
         }
   }
   this->items = 0;
}

static int comparisons = 0;

static int partition(Object** array, int left, int right, int pivotIndex, Object_Compare compare) {
   void* pivotValue = array[pivotIndex];
   swap(array, pivotIndex, right);
   int storeIndex = left;
   for (int i = left; i < right; i++) {
      comparisons++;
      if (compare(array[i], pivotValue) <= 0) {
         swap(array, i, storeIndex);
         storeIndex++;
      }
   }
   swap(array, storeIndex, right);
   return storeIndex;
}

static void quickSort(Object** array, int left, int right, Object_Compare compare) {
   if (left >= right)
      return;
   int pivotIndex = (left+right) / 2;
   int pivotNewIndex = partition(array, left, right, pivotIndex, compare);
   quickSort(array, left, pivotNewIndex - 1, compare);
   quickSort(array, pivotNewIndex + 1, right, compare);
}

// If I were to use only one sorting algorithm for both cases, it would probably be this one:
/*

static void combSort(Object** array, int left, int right, Object_Compare compare) {
   int gap = right - left;
   bool swapped = true;
   while ((gap > 1) || swapped) {
      if (gap > 1) {
         gap = (int)((double)gap / 1.247330950103979);
      }
      swapped = false;
      for (int i = left; gap + i <= right; i++) {
         comparisons++;
         if (compare(array[i], array[i+gap]) > 0) {
            swap(array, i, i+gap);
            swapped = true;
         }
      }
   }
}

*/

static void insertionSort(Object** array, int left, int right, Object_Compare compare) {
   for (int i = left+1; i <= right; i++) {
      void* t = array[i];
      int j = i - 1;
      while (j >= left) {
         comparisons++;
         if (compare(array[j], t) <= 0)
            break;
         array[j+1] = array[j];
         j--;
      }
      array[j+1] = t;
   }
}

void Vector_quickSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   quickSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_insertionSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   insertionSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

static void Vector_checkArraySize(Vector* this) {
   assert(Vector_isConsistent(this));
   if (this->items >= this->arraySize) {
      //int i;
      //i = this->arraySize;
      this->arraySize = this->items + this->growthRate;
      this->array = (Object**) xRealloc(this->array, sizeof(Object*) * this->arraySize);
      //for (; i < this->arraySize; i++)
      //   this->array[i] = NULL;
   }
   assert(Vector_isConsistent(this));
}

void Vector_insert(Vector* this, int idx, void* data_) {
   Object* data = data_;
   assert(idx >= 0);
   assert(Object_isA(data, this->type));
   assert(Vector_isConsistent(this));

   if (idx > this->items) {
      idx = this->items;
   }

   Vector_checkArraySize(this);
   //assert(this->array[this->items] == NULL);
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
   //assert (removed != NULL);
   this->items--;
   for (int i = idx; i < this->items; i++)
      this->array[i] = this->array[i+1];
   //this->array[this->items] = NULL;
   assert(Vector_isConsistent(this));
   return removed;
}

Object* Vector_remove(Vector* this, int idx) {
   Object* removed = Vector_take(this, idx);
   if (this->owner) {
      Object_delete(removed);
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
   Object* data = data_;
   assert(idx >= 0);
   assert(Object_isA((Object*)data, this->type));
   assert(Vector_isConsistent(this));

   Vector_checkArraySize(this);
   if (idx >= this->items) {
      this->items = idx+1;
   } else {
      if (this->owner) {
         Object* removed = this->array[idx];
         assert (removed != NULL);
         if (this->owner) {
            Object_delete(removed);
         }
      }
   }
   this->array[idx] = data;
   assert(Vector_isConsistent(this));
}

#ifdef DEBUG

inline Object* Vector_get(Vector* this, int idx) {
   assert(idx < this->items);
   assert(Vector_isConsistent(this));
   return this->array[idx];
}

#else

// In this case, Vector_get is defined as a macro in vector.h

#endif

#ifdef DEBUG

inline int Vector_size(Vector* this) {
   assert(Vector_isConsistent(this));
   return this->items;
}

#else

// In this case, Vector_size is defined as a macro in vector.h

#endif

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
   Object* data = data_;
   assert(Object_isA((Object*)data, this->type));
   assert(Vector_isConsistent(this));
   int i = this->items;
   Vector_set(this, this->items, data);
   assert(this->items == i+1); (void)(i);
   assert(Vector_isConsistent(this));
}

inline int Vector_indexOf(Vector* this, void* search_, Object_Compare compare) {
   Object* search = search_;
   assert(Object_isA((Object*)search, this->type));
   assert(compare);
   assert(Vector_isConsistent(this));
   for (int i = 0; i < this->items; i++) {
      Object* o = (Object*)this->array[i];
      assert(o);
      if (compare(search, o) == 0)
         return i;
   }
   return -1;
}

void Vector_splice(Vector* this, Vector* from) {
   assert(Vector_isConsistent(this));
   assert(Vector_isConsistent(from));
   assert(!(this->owner && from->owner));

   int olditmes = this->items;
   this->items += from->items;
   Vector_checkArraySize(this);
   for (int j = 0; j < from->items; j++)
      this->array[olditmes + j] = from->array[j];
}
