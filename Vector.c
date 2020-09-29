/*
htop - Vector.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"

#include "Macros.h"

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
   Object* pivotValue = array[pivotIndex];
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

static void merge(Object** array, int left, int middle, int right, Object_Compare compare)
{
   const int n1 = middle - left + 1;
   const int n2 = right - middle;
   Object* L[n1];
   Object* R[n2];

   for (int i = 0; i < n1; i++)
      L[i] = array[left + i];
   for (int i = 0; i < n2; i++)
      R[i] = array[middle + 1 + i];

   int i = 0;
   int j = 0;
   int k = left;
   while (i < n1 && j < n2) {
      comparisons++;
      if (compare(L[i], R[j]) <= 0) {
         array[k] = L[i];
         i++;
      }
      else {
         array[k] = R[j];
         j++;
      }
      k++;
   }

   while (i < n1) {
      array[k] = L[i];
      i++;
      k++;
   }

   while (j < n2) {
      array[k] = R[j];
      j++;
      k++;
   }
}

static void mergeSort(Object** array, int left, int right, Object_Compare compare)
{
   if (left < right) {
      int middle = left + (right - left) / 2;

      mergeSort(array, left, middle, compare);
      mergeSort(array, middle + 1, right, compare);

      merge(array, left, middle, right, compare);
   }
}

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

static void insertionSort(Object** array, int left, int right, Object_Compare compare) {
   for (int i = left+1; i <= right; i++) {
      Object* t = array[i];
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

#define TIM_RUN 32

static void timSort(Object** array, ATTR_UNUSED int unused, int right, Object_Compare compare) {
   for (int i = 0; i <= right; i += TIM_RUN)
      insertionSort(array, i, MINIMUM(i + (TIM_RUN - 1), right), compare);

   for (int size = TIM_RUN; size <= right; size = 2 * size) {

      for (int l = 0; l <= right; l += 2 * size) {

         int mid = l + size - 1;
         int r = MINIMUM(l + 2 * size - 1, right);

         if (mid >= r)
            continue;

         merge(array, l, mid, r, compare);
      }
   }
}

#include <time.h>
static long unsigned int now_us(void) {
   struct timespec ts;

   clock_gettime(CLOCK_MONOTONIC, &ts);

   return (long unsigned int) ts.tv_sec * 1000000 + (long unsigned int) ts.tv_nsec / 1000;
}

void Vector_quickSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   quickSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_mergeSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   mergeSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_combSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   combSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_insertionSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   insertionSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_timSort(Vector* this) {
   assert(this->type->compare);
   assert(Vector_isConsistent(this));
   timSort(this->array, 0, this->items - 1, this->type->compare);
   assert(Vector_isConsistent(this));
}

void Vector_testSort(Vector* this) {
   size_t origSize = this->items * sizeof(Object*);
   Object** origData = xMalloc(origSize);
   memcpy(origData, this->array, origSize);


   const char *winner;
   long unsigned int timeWinner;
   long unsigned int startTime, endTime;

   /* insertion sort */
   comparisons = 0;
   startTime = now_us();
   Vector_insertionSort(this);
   endTime = now_us();
   int comparisonsIS = comparisons;
   long unsigned int timeIS = endTime - startTime;
   winner = "IS";
   timeWinner = timeIS;

   memcpy(this->array, origData, origSize);

   /* quick sort */
   comparisons = 0;
   startTime = now_us();
   Vector_quickSort(this);
   endTime = now_us();
   int comparisonsQS = comparisons;
   long unsigned int timeQS = endTime - startTime;
   if (timeQS < timeWinner) {
      winner = "QS";
      timeWinner = timeQS;
   }

   memcpy(this->array, origData, origSize);

   /* merge sort */
   comparisons = 0;
   startTime = now_us();
   Vector_mergeSort(this);
   endTime = now_us();
   int comparisonsMS = comparisons;
   long unsigned int timeMS = endTime - startTime;
   if (timeMS < timeWinner) {
      winner = "MS";
      timeWinner = timeMS;
   }

   memcpy(this->array, origData, origSize);

   /* comb sort */
   comparisons = 0;
   startTime = now_us();
   Vector_combSort(this);
   endTime = now_us();
   int comparisonsCS = comparisons;
   long unsigned int timeCS = endTime - startTime;
   if (timeCS < timeWinner) {
      winner = "CS";
      timeWinner = timeCS;
   }

   memcpy(this->array, origData, origSize);

   /* tim sort */
   comparisons = 0;
   startTime = now_us();
   Vector_timSort(this);
   endTime = now_us();
   int comparisonsTS = comparisons;
   long unsigned int timeTS = endTime - startTime;
   if (timeTS < timeWinner) {
      winner = "TS";
      timeWinner = timeTS;
   }

   free(origData);

   fprintf(stderr, "Vector_testSort(): items=%d IS=%d/%luus QS=%d/%luus MS=%d/%luus CS=%d/%luus TS=%d/%luus   winner=%s\n",
           this->items,
           comparisonsIS, timeIS,
           comparisonsQS, timeQS,
           comparisonsMS, timeMS,
           comparisonsCS, timeCS,
           comparisonsTS, timeTS,
           winner);
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
      Object* o = this->array[i];
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
