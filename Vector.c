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
#include "generic/Sort.h"


typedef struct VectorSortContext_ {
   Object_Compare compare;
   void* compareContext;
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
static int Vector_sortCompare(const void* p1, const void* p2, void* context) {
   VectorSortContext* vc = (VectorSortContext*) context;

   return vc->compare(*(const void* const*)p1, *(const void* const*)p2, vc->compareContext);
}

ATTR_NONNULL_N(1)
void Vector_sort(Vector* this, Object_Compare compare, void* context) {
   VectorSortContext vc = {
      .compare = compare ? compare : this->type->compare,
      .compareContext = context ? context : this,
   };
   assert(vc.compare);
   assert(Vector_isConsistent(this));

   Sort_sort(this->array, this->items, sizeof(*this->array), Vector_sortCompare, &vc);

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
      if (compare(search, o, NULL) == 0) {
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
