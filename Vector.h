#ifndef HEADER_Vector
#define HEADER_Vector
/*
htop - Vector.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Object.h"

#define swap(a_,x_,y_) do{ void* tmp_ = a_[x_]; a_[x_] = a_[y_]; a_[y_] = tmp_; }while(0)

#ifndef DEFAULT_SIZE
#define DEFAULT_SIZE -1
#endif

typedef struct Vector_ {
   Object **array;
   ObjectClass* type;
   int arraySize;
   int growthRate;
   int items;
   bool owner;
} Vector;

Vector* Vector_new(ObjectClass* type, bool owner, int size);

void Vector_delete(Vector* this);

#ifdef DEBUG

int Vector_count(Vector* this);

#endif

void Vector_prune(Vector* this);

void Vector_quickSort(Vector* this);

void Vector_insertionSort(Vector* this);

void Vector_insert(Vector* this, int idx, void* data_);

Object* Vector_take(Vector* this, int idx);

Object* Vector_remove(Vector* this, int idx);

void Vector_moveUp(Vector* this, int idx);

void Vector_moveDown(Vector* this, int idx);

void Vector_set(Vector* this, int idx, void* data_);

#ifdef DEBUG

Object* Vector_get(Vector* this, int idx);

#else

#define Vector_get(v_, idx_) ((v_)->array[idx_])

#endif

#ifdef DEBUG

int Vector_size(Vector* this);

#else

#define Vector_size(v_) ((v_)->items)

#endif

void Vector_add(Vector* this, void* data_);

int Vector_indexOf(Vector* this, void* search_, Object_Compare compare);

void Vector_splice(Vector* this, Vector* from);

#endif
