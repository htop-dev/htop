#ifndef HEADER_Vector
#define HEADER_Vector
/*
htop - Vector.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"

#include <stdbool.h>


#ifndef DEFAULT_SIZE
#define DEFAULT_SIZE (-1)
#endif

typedef struct Vector_ {
   Object** array;
   const ObjectClass* type;
   int arraySize;
   int growthRate;
   int items;
   /* lowest index of a pending soft remove/delete operation,
      used to speed up compaction */
   int dirty_index;
   /* count of soft deletes, required for Vector_count to work in debug mode */
   int dirty_count;
   bool owner;
} Vector;

Vector* Vector_new(const ObjectClass* type, bool owner, int size);

void Vector_delete(Vector* this);

void Vector_prune(Vector* this);

void Vector_quickSortCustomCompare(Vector* this, Object_Compare compare);
static inline void Vector_quickSort(Vector* this) {
   Vector_quickSortCustomCompare(this, this->type->compare);
}

void Vector_insertionSort(Vector* this);

void Vector_insert(Vector* this, int idx, void* data_);

Object* Vector_take(Vector* this, int idx);

Object* Vector_remove(Vector* this, int idx);

/* Vector_softRemove marks the item at index idx for deletion without
   reclaiming any space. If owned, the item is immediately freed.

   Vector_compact must be called to reclaim space.*/
Object* Vector_softRemove(Vector* this, int idx);

/* Vector_compact reclaims space free'd up by Vector_softRemove, if any. */
void Vector_compact(Vector* this);

void Vector_moveUp(Vector* this, int idx);

void Vector_moveDown(Vector* this, int idx);

void Vector_set(Vector* this, int idx, void* data_);

#ifndef NDEBUG

Object* Vector_get(const Vector* this, int idx);
int Vector_size(const Vector* this);

/* Vector_countEquals returns true if the number of non-NULL items
   in the Vector is equal to expectedCount. This is only for debugging
   and consistency checks. */
bool Vector_countEquals(const Vector* this, unsigned int expectedCount);

#else /* NDEBUG */

static inline Object* Vector_get(const Vector* this, int idx) {
   return this->array[idx];
}

static inline int Vector_size(const Vector* this) {
   return this->items;
}

#endif /* NDEBUG */

static inline const ObjectClass* Vector_type(const Vector* this) {
   return this->type;
}

void Vector_add(Vector* this, void* data_);

int Vector_indexOf(const Vector* this, const void* search_, Object_Compare compare);

void Vector_splice(Vector* this, Vector* from);

#endif
