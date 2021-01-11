/*
htop - Vector.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"

#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "XUtils.h"


Vector* Vector_new(const ObjectClass* type, bool owner, int size) {
   Vector* this;

   if (size == DEFAULT_SIZE) {
      size = 10;
   }

   HTOP_ASSERT(size > 0);
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
      for (int i = 0; i < this->items; i++) {
         if (this->array[i]) {
            Object_delete(this->array[i]);
         }
      }
   }
   free(this->array);
   free(this);
}

#ifndef NDEBUG

static bool Vector_isConsistent(const Vector* this) {
   HTOP_ASSERT(this->items <= this->arraySize);

   if (this->owner) {
      for (int i = 0; i < this->items; i++) {
         if (!this->array[i]) {
            return false;
         }
      }
   }

   return true;
}

unsigned int Vector_count(const Vector* this) {
   unsigned int items = 0;
   for (int i = 0; i < this->items; i++) {
      if (this->array[i]) {
         items++;
      }
   }
   HTOP_ASSERT(items == (unsigned int)this->items);
   return items;
}

Object* Vector_get(const Vector* this, int idx) {
   HTOP_ASSERT(idx >= 0 && idx < this->items);
   HTOP_ASSERT(this->array[idx]);
   HTOP_ASSERT(Object_isA(this->array[idx], this->type));
   return this->array[idx];
}

int Vector_size(const Vector* this) {
   HTOP_ASSERT(Vector_isConsistent(this));
   return this->items;
}

#endif /* NDEBUG */

void Vector_prune(Vector* this) {
   HTOP_ASSERT(Vector_isConsistent(this));
   if (this->owner) {
      for (int i = 0; i < this->items; i++)
         if (this->array[i]) {
            Object_delete(this->array[i]);
            //this->array[i] = NULL;
         }
   }
   this->items = 0;
}

//static int comparisons = 0;

static void swap(Object** array, int indexA, int indexB) {
   HTOP_ASSERT(indexA >= 0);
   HTOP_ASSERT(indexB >= 0);
   Object* tmp = array[indexA];
   array[indexA] = array[indexB];
   array[indexB] = tmp;
}

static int partition(Object** array, int left, int right, int pivotIndex, Object_Compare compare) {
   const Object* pivotValue = array[pivotIndex];
   swap(array, pivotIndex, right);
   int storeIndex = left;
   for (int i = left; i < right; i++) {
      //comparisons++;
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

   int pivotIndex = (left + right) / 2;
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
   for (int i = left + 1; i <= right; i++) {
      Object* t = array[i];
      int j = i - 1;
      while (j >= left) {
         //comparisons++;
         if (compare(array[j], t) <= 0)
            break;

         array[j + 1] = array[j];
         j--;
      }
      array[j + 1] = t;
   }
}

void Vector_quickSortCustomCompare(Vector* this, Object_Compare compare) {
   HTOP_ASSERT(compare);
   HTOP_ASSERT(Vector_isConsistent(this));
   quickSort(this->array, 0, this->items - 1, compare);
   HTOP_ASSERT(Vector_isConsistent(this));
}

void Vector_insertionSort(Vector* this) {
   HTOP_ASSERT(this->type->compare);
   HTOP_ASSERT(Vector_isConsistent(this));
   insertionSort(this->array, 0, this->items - 1, this->type->compare);
   HTOP_ASSERT(Vector_isConsistent(this));
}

static void Vector_checkArraySize(Vector* this) {
   HTOP_ASSERT(Vector_isConsistent(this));
   if (this->items >= this->arraySize) {
      //int i;
      //i = this->arraySize;
      this->arraySize = this->items + this->growthRate;
      this->array = (Object**) xRealloc(this->array, sizeof(Object*) * this->arraySize);
      //for (; i < this->arraySize; i++)
      //   this->array[i] = NULL;
   }
   HTOP_ASSERT(Vector_isConsistent(this));
}

void Vector_insert(Vector* this, int idx, void* data_) {
   Object* data = data_;
   HTOP_ASSERT(idx >= 0);
   HTOP_ASSERT(Object_isA(data, this->type));
   HTOP_ASSERT(Vector_isConsistent(this));

   if (idx > this->items) {
      idx = this->items;
   }

   Vector_checkArraySize(this);
   //HTOP_ASSERT(this->array[this->items] == NULL);
   if (idx < this->items) {
      memmove(&this->array[idx + 1], &this->array[idx], (this->items - idx) * sizeof(this->array[0]));
   }
   this->array[idx] = data;
   this->items++;
   HTOP_ASSERT(Vector_isConsistent(this));
}

Object* Vector_take(Vector* this, int idx) {
   HTOP_ASSERT(idx >= 0 && idx < this->items);
   HTOP_ASSERT(Vector_isConsistent(this));
   Object* removed = this->array[idx];
   HTOP_ASSERT(removed);
   this->items--;
   if (idx < this->items) {
      memmove(&this->array[idx], &this->array[idx + 1], (this->items - idx) * sizeof(this->array[0]));
   }
   //this->array[this->items] = NULL;
   HTOP_ASSERT(Vector_isConsistent(this));
   return removed;
}

Object* Vector_remove(Vector* this, int idx) {
   Object* removed = Vector_take(this, idx);
   if (this->owner) {
      Object_delete(removed);
      return NULL;
   } else {
      return removed;
   }
}

void Vector_moveUp(Vector* this, int idx) {
   HTOP_ASSERT(idx >= 0 && idx < this->items);
   HTOP_ASSERT(Vector_isConsistent(this));

   if (idx == 0)
      return;

   Object* temp = this->array[idx];
   this->array[idx] = this->array[idx - 1];
   this->array[idx - 1] = temp;
}

void Vector_moveDown(Vector* this, int idx) {
   HTOP_ASSERT(idx >= 0 && idx < this->items);
   HTOP_ASSERT(Vector_isConsistent(this));

   if (idx == this->items - 1)
      return;

   Object* temp = this->array[idx];
   this->array[idx] = this->array[idx + 1];
   this->array[idx + 1] = temp;
}

void Vector_set(Vector* this, int idx, void* data_) {
   Object* data = data_;
   HTOP_ASSERT(idx >= 0);
   HTOP_ASSERT(Object_isA(data, this->type));
   HTOP_ASSERT(Vector_isConsistent(this));

   Vector_checkArraySize(this);
   if (idx >= this->items) {
      this->items = idx + 1;
   } else {
      if (this->owner) {
         Object* removed = this->array[idx];
         HTOP_ASSERT(removed != NULL);
         Object_delete(removed);
      }
   }
   this->array[idx] = data;
   HTOP_ASSERT(Vector_isConsistent(this));
}

/*

static void Vector_merge(Vector* this, Vector* v2) {
   int i;
   HTOP_ASSERT(Vector_isConsistent(this));

   for (i = 0; i < v2->items; i++)
      Vector_add(this, v2->array[i]);
   v2->items = 0;
   Vector_delete(v2);
   HTOP_ASSERT(Vector_isConsistent(this));
}

*/

void Vector_add(Vector* this, void* data_) {
   Object* data = data_;
   HTOP_ASSERT(Object_isA(data, this->type));
   HTOP_ASSERT(Vector_isConsistent(this));
   int i = this->items;
   Vector_set(this, this->items, data);
   HTOP_ASSERT(this->items == i + 1); (void)(i);
   HTOP_ASSERT(Vector_isConsistent(this));
}

int Vector_indexOf(const Vector* this, const void* search_, Object_Compare compare) {
   const Object* search = search_;
   HTOP_ASSERT(Object_isA(search, this->type));
   HTOP_ASSERT(compare);
   HTOP_ASSERT(Vector_isConsistent(this));
   for (int i = 0; i < this->items; i++) {
      const Object* o = this->array[i];
      HTOP_ASSERT(o);
      if (compare(search, o) == 0) {
         return i;
      }
   }
   return -1;
}

void Vector_splice(Vector* this, Vector* from) {
   HTOP_ASSERT(Vector_isConsistent(this));
   HTOP_ASSERT(Vector_isConsistent(from));
   HTOP_ASSERT(!(this->owner && from->owner));

   int olditems = this->items;
   this->items += from->items;
   Vector_checkArraySize(this);
   for (int j = 0; j < from->items; j++) {
      this->array[olditems + j] = from->array[j];
   }
}
