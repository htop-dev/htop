/*
htop - Vector.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Vector.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "XUtils.h"


typedef int(*CompareWithContext)(const void*, const void*, void*);

typedef struct VectorSortContext_ {
   Object_Compare compare;
} VectorSortContext;

Vector* Vector_new(const ObjectClass* type, bool owner, int size) {
   Vector* this;

   assert(size > 0);
   this = xMalloc(sizeof(Vector));
   *this = (Vector) {
      .growthRate = size,
      .array = xCalloc(size, sizeof(Object*)),
      .arraySize = size,
      .items = 0,
      .type = type,
      .owner = owner,
      .isDirty = false,
   };
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
   assert(this->items <= this->arraySize);
   assert(!this->isDirty);

   return true;
}

bool Vector_countEquals(const Vector* this, unsigned int expectedCount) {
   unsigned int n = 0;
   for (int i = 0; i < this->items; i++) {
      if (this->array[i]) {
         n++;
      }
   }
   return n == expectedCount;
}

Object* Vector_get(const Vector* this, size_t idx) {
   assert(idx < (size_t)this->arraySize);
   assert(idx < (size_t)this->items);
   assert(this->array[idx]);
   assert(Object_isA(this->array[idx], this->type));
   return this->array[idx];
}

int Vector_size(const Vector* this) {
   assert(Vector_isConsistent(this));
   return this->items;
}

#endif /* NDEBUG */

void Vector_prune(Vector* this) {
   assert(Vector_isConsistent(this));
   if (this->owner) {
      for (int i = 0; i < this->items; i++) {
         if (this->array[i]) {
            Object_delete(this->array[i]);
         }
      }
   }
   this->items = 0;
   this->isDirty = false;
   memset(this->array, '\0', this->arraySize * sizeof(Object*));
}

ATTR_NONNULL
static void swapByte(char* p1, char* p2) {
    char temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

ATTR_NONNULL
static void rotate(void* buffer, size_t leftSize, size_t rightSize) {
   if (rightSize == 0)
      return;

   char* p1 = buffer;
   char* p2 = p1 + leftSize;
   char* mid = p2;
   const char* const end = mid + rightSize;

   while (true) {
      // Ensure there is no arithmetic overflow on input.
      assert(p1 <= mid);
      assert(mid <= p2);
      assert(p2 <= end);

      if (p2 >= end) {
         assert(mid < end);
         p2 = mid;
      }

      if (p1 >= p2)
         break;

      if (p1 >= mid)
         mid = p2;

      swapByte(p1, p2);
      p1 += 1;
      p2 += 1;
   }
}

ATTR_NONNULL_N(1, 5)
static void mergeRuns(void* array, size_t leftLen, size_t rightLen, size_t size, CompareWithContext compare, void* context) {
   assert(size > 0);
   if (leftLen == 0 || rightLen == 0 || size == 0)
      return;

   assert(leftLen <= SIZE_MAX / size);
   assert(rightLen <= SIZE_MAX / size);

   char* p1 = array;
   char* p2 = p1 + leftLen * size;
   char* mid = p2;
   const char* const end = mid + rightLen * size;

   for (size_t limit = (leftLen + rightLen) / 2; limit > 0; limit--) {
      // Ensure there is no arithmetic overflow on input.
      assert(p1 <= mid);
      assert(mid <= p2);
      assert(p2 <= end);

      if (p1 >= mid || p2 >= end)
         break;

      if (compare(p1, p2, context) <= 0) {
         p1 += size;
      } else {
         p2 += size;
      }
   }

   rotate(p1, (size_t)(mid - p1), (size_t)(p2 - mid));

   leftLen = (size_t)(p1 - (char*)array) / size;
   rightLen = (size_t)(p2 - mid) / size;
   mergeRuns(array, leftLen, rightLen, size, compare, context);

   leftLen = (size_t)(mid - p1) / size;
   rightLen = (size_t)(end - p2) / size;
   mergeRuns(p1 + (p2 - mid), leftLen, rightLen, size, compare, context);
}

ATTR_NONNULL_N(1, 5)
static size_t mergeSortSubarray(void* array, size_t unsortedLen, size_t limit, size_t size, CompareWithContext compare, void* context) {
   assert(size > 0);
   if (size == 0)
      return 0;

   // The initial level of this function call must set "limit" to 0. Subsequent
   // levels of recursion will have "limit" no less than the previous level.

   // A run is a sorted subarray. Each recursive call of this function keeps
   // the lengths of two runs. At most O(log(n)) lengths of runs will be
   // tracked on the call stack.
   size_t runLen[3] = {0};
   while (unsortedLen > 0) {
      size_t totalLen = unsortedLen;
      assert(totalLen <= SIZE_MAX / size);
      while (true) {
         --unsortedLen;

         const char* p2 = (const char*)array + unsortedLen * size;
         // Ensure there is no arithmetic overflow on input.
         assert(p2 > (const char*)array);

         if (unsortedLen < limit)
            return 0;

         if (unsortedLen == 0 || compare(p2 - 1 * size, p2, context) > 0) {
            break;
         }
      }
      runLen[1] = totalLen - unsortedLen;

      bool reachesLimit = false;

      assert(runLen[2] > 0 || runLen[0] == 0);
      if (runLen[2] > 0) {
         size_t nextLimit = limit;
         if (unsortedLen > runLen[2] + limit) {
            nextLimit = unsortedLen - runLen[2];
         } else {
            reachesLimit = true;
         }

         runLen[0] = mergeSortSubarray(array, unsortedLen, nextLimit, size, compare, context);
         unsortedLen -= runLen[0];

         char* p1 = (char*)array + unsortedLen * size;
         mergeRuns(p1, runLen[0], runLen[1], size, compare, context);
         runLen[1] += runLen[0];
         runLen[0] = 0;

         mergeRuns(p1, runLen[1], runLen[2], size, compare, context);
      }
      runLen[2] += runLen[1];
      runLen[1] = 0;

      if (reachesLimit) {
         break;
      }
   }
   return runLen[2];
}

ATTR_NONNULL
static int Vector_sortCompare(const void* p1, const void* p2, void* context) {
   VectorSortContext* vc = (VectorSortContext*) context;

   return vc->compare(*(const void* const*)p1, *(const void* const*)p2);
}

ATTR_NONNULL_N(1)
void Vector_sort(Vector* this, Object_Compare compare) {
   VectorSortContext vc = {
      .compare = compare ? compare : this->type->compare,
   };
   assert(vc.compare);
   assert(Vector_isConsistent(this));

   (void)mergeSortSubarray(this->array, this->items, 0, sizeof(*this->array), Vector_sortCompare, &vc);

   assert(Vector_isConsistent(this));
}

static void Vector_resizeIfNecessary(Vector* this, int newSize) {
   assert(newSize >= 0);
   if (newSize > this->arraySize) {
      assert(Vector_isConsistent(this));
      int oldSize = this->arraySize;
      this->arraySize = newSize + this->growthRate;
      this->array = (Object**)xReallocArrayZero(this->array, oldSize, this->arraySize, sizeof(Object*));
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

   Vector_resizeIfNecessary(this, this->items + 1);
   //assert(this->array[this->items] == NULL);
   if (idx < this->items) {
      memmove(&this->array[idx + 1], &this->array[idx], (this->items - idx) * sizeof(this->array[0]));
   }
   this->array[idx] = data;
   this->items++;
   assert(Vector_isConsistent(this));
}

Object* Vector_take(Vector* this, int idx) {
   assert(idx >= 0 && idx < this->items);
   assert(Vector_isConsistent(this));
   Object* removed = this->array[idx];
   assert(removed);
   this->items--;
   if (idx < this->items) {
      memmove(&this->array[idx], &this->array[idx + 1], (this->items - idx) * sizeof(this->array[0]));
   }
   this->array[this->items] = NULL;
   assert(Vector_isConsistent(this));
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

Object* Vector_softRemove(Vector* this, int idx) {
   assert(idx >= 0 && idx < this->items);

   Object* removed = this->array[idx];
   assert(removed);
   if (removed) {
      this->array[idx] = NULL;

      this->isDirty = true;

      if (this->owner) {
         Object_delete(removed);
         return NULL;
      }
   }

   return removed;
}

void Vector_compact(Vector* this, int dirtyIndex) {
   if (!this->isDirty)
      return;

   assert(0 <= dirtyIndex);
   if (dirtyIndex >= this->items)
      return;

   assert(!this->array[dirtyIndex]);

   for (int i = dirtyIndex + 1; i < this->items; i++) {
      if (this->array[i]) {
         this->array[dirtyIndex++] = this->array[i];
      }
   }
   int dirtyCount = this->items - dirtyIndex;
   memset(&this->array[dirtyIndex], 0, dirtyCount * sizeof(this->array[0]));

   this->items = dirtyIndex;
   this->isDirty = false;

   assert(Vector_isConsistent(this));
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
   assert(Object_isA(data, this->type));
   assert(Vector_isConsistent(this));

   Vector_resizeIfNecessary(this, idx + 1);
   if (idx >= this->items) {
      this->items = idx + 1;
   } else {
      if (this->owner) {
         Object* removed = this->array[idx];
         if (removed != NULL) {
            Object_delete(removed);
         }
      }
   }
   this->array[idx] = data;
   assert(Vector_isConsistent(this));
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
   Object* data = data_;
   assert(Object_isA(data, this->type));
   assert(Vector_isConsistent(this));
   int i = this->items;
   Vector_set(this, this->items, data);
   assert(this->items == i + 1); (void)(i);
   assert(Vector_isConsistent(this));
}

int Vector_indexOf(const Vector* this, const void* search_, Object_Compare compare) {
   const Object* search = search_;
   assert(Object_isA(search, this->type));
   assert(compare);
   assert(Vector_isConsistent(this));
   for (int i = 0; i < this->items; i++) {
      const Object* o = this->array[i];
      assert(o);
      if (compare(search, o) == 0) {
         return i;
      }
   }
   return -1;
}

void Vector_splice(Vector* this, Vector* from) {
   assert(Vector_isConsistent(this));
   assert(Vector_isConsistent(from));
   assert(!this->owner);

   int olditems = this->items;
   Vector_resizeIfNecessary(this, this->items + from->items);
   this->items += from->items;
   for (int j = 0; j < from->items; j++) {
      this->array[olditems + j] = from->array[j];
   }
}
