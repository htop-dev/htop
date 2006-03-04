/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Hashtable.h"

#include <stdlib.h>
#include <stdbool.h>

#include "debug.h"

/*{
typedef struct Hashtable_ Hashtable;

typedef void(*Hashtable_PairFunction)(int, void*, void*);
typedef int(*Hashtable_HashAlgorithm)(Hashtable*, int);

typedef struct HashtableItem {
   int key;
   void* value;
   struct HashtableItem* next;
} HashtableItem;

struct Hashtable_ {
   int size;
   HashtableItem** buckets;
   int items;
   Hashtable_HashAlgorithm hashAlgorithm;
   bool owner;
};
}*/

HashtableItem* HashtableItem_new(int key, void* value) {
   HashtableItem* this;
   
   this = (HashtableItem*) malloc(sizeof(HashtableItem));
   this->key = key;
   this->value = value;
   this->next = NULL;
   return this;
}

Hashtable* Hashtable_new(int size, bool owner) {
   Hashtable* this;
   
   this = (Hashtable*) malloc(sizeof(Hashtable));
   this->size = size;
   this->buckets = (HashtableItem**) calloc(sizeof(HashtableItem*), size);
   this->hashAlgorithm = Hashtable_hashAlgorithm;
   this->owner = owner;
   return this;
}

int Hashtable_hashAlgorithm(Hashtable* this, int key) {
   return (key % this->size);
}

void Hashtable_delete(Hashtable* this) {
   for (int i = 0; i < this->size; i++) {
      HashtableItem* walk = this->buckets[i];
      while (walk != NULL) {
         if (this->owner)
            free(walk->value);
         HashtableItem* savedWalk = walk;
         walk = savedWalk->next;
         free(savedWalk);
      }
   }
   free(this->buckets);
   free(this);
}

inline int Hashtable_size(Hashtable* this) {
   return this->items;
}

void Hashtable_put(Hashtable* this, int key, void* value) {
   int index = this->hashAlgorithm(this, key);
   HashtableItem** bucketPtr = &(this->buckets[index]);
   while (true)
      if (*bucketPtr == NULL) {
         *bucketPtr = HashtableItem_new(key, value);
         this->items++;
         break;
      } else if ((*bucketPtr)->key == key) {
         if (this->owner)
            free((*bucketPtr)->value);
         (*bucketPtr)->value = value;
         break;
      } else
         bucketPtr = &((*bucketPtr)->next);
}

void* Hashtable_remove(Hashtable* this, int key) {
   int index = this->hashAlgorithm(this, key);
   HashtableItem** bucketPtr = &(this->buckets[index]);
   while (true)
      if (*bucketPtr == NULL) {
         return NULL;
         break;
      } else if ((*bucketPtr)->key == key) {
         void* savedValue = (*bucketPtr)->value;
         HashtableItem* savedNext = (*bucketPtr)->next;
         free(*bucketPtr);
         (*bucketPtr) = savedNext;
         this->items--;
         if (this->owner) {
            free(savedValue);
            return NULL;
         } else {
            return savedValue;
         }
      } else
         bucketPtr = &((*bucketPtr)->next);
}

inline void* Hashtable_get(Hashtable* this, int key) {
   int index = this->hashAlgorithm(this, key);
   HashtableItem* bucketPtr = this->buckets[index];
   while (true)
      if (bucketPtr == NULL) {
         return NULL;
      } else if (bucketPtr->key == key) {
         return bucketPtr->value;
      } else
         bucketPtr = bucketPtr->next;
}

void Hashtable_foreach(Hashtable* this, Hashtable_PairFunction f, void* userData) {
   for (int i = 0; i < this->size; i++) {
      HashtableItem* walk = this->buckets[i];
      while (walk != NULL) {
         f(walk->key, walk->value, userData);
         walk = walk->next;
      }
   }
}
