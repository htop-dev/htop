/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Hashtable.h"
#include "XUtils.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>


#ifndef NDEBUG

static void Hashtable_dump(const Hashtable* this) {
   fprintf(stderr, "Hashtable %p: size=%u items=%u owner=%s\n",
           (const void*)this,
           this->size,
           this->items,
           this->owner ? "yes" : "no");

   unsigned int items = 0;
   for (unsigned int i = 0; i < this->size; i++) {
      fprintf(stderr, "  item %5u: key = %5u probe = %2u value = %p\n",
              i,
              this->buckets[i].key,
              this->buckets[i].probe,
              this->buckets[i].value ? (const void*)this->buckets[i].value : "(nil)");

      if (this->buckets[i].value)
         items++;
   }

   fprintf(stderr, "Hashtable %p: items=%u counted=%u\n",
           (const void*)this,
           this->items,
           items);
}

static bool Hashtable_isConsistent(const Hashtable* this) {
   unsigned int items = 0;
   for (unsigned int i = 0; i < this->size; i++) {
      if (this->buckets[i].value)
         items++;
   }
   bool res = items == this->items;
   if (!res)
      Hashtable_dump(this);
   return res;
}

unsigned int Hashtable_count(const Hashtable* this) {
   unsigned int items = 0;
   for (unsigned int i = 0; i < this->size; i++) {
      if (this->buckets[i].value)
         items++;
   }
   assert(items == this->items);
   return items;
}

#endif /* NDEBUG */

/* https://oeis.org/A014234 */
static const uint64_t OEISprimes[] = {
   2, 3, 7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191,
   16381, 32749, 65521, 131071, 262139, 524287, 1048573,
   2097143, 4194301, 8388593, 16777213, 33554393,
   67108859, 134217689, 268435399, 536870909, 1073741789,
   2147483647, 4294967291, 8589934583, 17179869143,
   34359738337, 68719476731, 137438953447
};

static uint64_t nextPrime(unsigned int n) {
   assert(n <= OEISprimes[ARRAYSIZE(OEISprimes) - 1]);

   for (unsigned int i = 0; i < ARRAYSIZE(OEISprimes); i++) {
      if (n <= OEISprimes[i])
         return OEISprimes[i];
   }

   return OEISprimes[ARRAYSIZE(OEISprimes) - 1];
}

Hashtable* Hashtable_new(unsigned int size, bool owner) {
   Hashtable* this;

   this = xMalloc(sizeof(Hashtable));
   this->items = 0;
   this->size = size ? nextPrime(size) : 13;
   this->buckets = (HashtableItem*) xCalloc(this->size, sizeof(HashtableItem));
   this->owner = owner;
   assert(Hashtable_isConsistent(this));
   return this;
}

void Hashtable_delete(Hashtable* this) {

   assert(Hashtable_isConsistent(this));

   if (this->owner) {
      for (unsigned int i = 0; i < this->size; i++)
         free(this->buckets[i].value);
   }

   free(this->buckets);
   free(this);
}

static void insert(Hashtable* this, hkey_t key, void* value) {
   unsigned int index = key % this->size;
   unsigned int probe = 0;
#ifndef NDEBUG
   unsigned int origIndex = index;
#endif

   for (;;) {
      if (!this->buckets[index].value) {
         this->items++;
         this->buckets[index].key = key;
         this->buckets[index].probe = probe;
         this->buckets[index].value = value;
         return;
      }

      if (this->buckets[index].key == key) {
         if (this->owner && this->buckets[index].value != value)
            free(this->buckets[index].value);
         this->buckets[index].value = value;
         return;
      }

      /* Robin Hood swap */
      if (probe > this->buckets[index].probe) {
         HashtableItem tmp = this->buckets[index];

         this->buckets[index].key = key;
         this->buckets[index].probe = probe;
         this->buckets[index].value = value;

         key = tmp.key;
         probe = tmp.probe;
         value = tmp.value;
      }

      index = (index + 1) % this->size;
      probe++;

      assert(index != origIndex);
   }
}

void Hashtable_setSize(Hashtable* this, unsigned int size) {

   assert(Hashtable_isConsistent(this));

   if (size <= this->items)
      return;

   HashtableItem* oldBuckets = this->buckets;
   unsigned int oldSize = this->size;

   this->size = nextPrime(size);
   this->buckets = (HashtableItem*) xCalloc(this->size, sizeof(HashtableItem));
   this->items = 0;

   /* rehash */
   for (unsigned int i = 0; i < oldSize; i++) {
      if (!oldBuckets[i].value)
         continue;

      insert(this, oldBuckets[i].key, oldBuckets[i].value);
   }

   free(oldBuckets);

   assert(Hashtable_isConsistent(this));
}

void Hashtable_put(Hashtable* this, hkey_t key, void* value) {

   assert(Hashtable_isConsistent(this));
   assert(this->size > 0);
   assert(value);

   /* grow on load-factor > 0.7 */
   if (10 * this->items > 7 * this->size)
      Hashtable_setSize(this, 2 * this->size);

   insert(this, key, value);

   assert(Hashtable_isConsistent(this));
   assert(Hashtable_get(this, key) != NULL);
   assert(this->size > this->items);
}

void* Hashtable_remove(Hashtable* this, hkey_t key) {
   unsigned int index = key % this->size;
   unsigned int probe = 0;
#ifndef NDEBUG
   unsigned int origIndex = index;
#endif

   assert(Hashtable_isConsistent(this));

   void* res = NULL;

   while (this->buckets[index].value) {
      if (this->buckets[index].key == key) {
         if (this->owner) {
            free(this->buckets[index].value);
         } else {
            res = this->buckets[index].value;
         }

         unsigned int next = (index + 1) % this->size;

         while (this->buckets[next].value && this->buckets[next].probe > 0) {
            this->buckets[index] = this->buckets[next];
            this->buckets[index].probe -= 1;

            index = next;
            next = (index + 1) % this->size;
         }

         /* set empty after backward shifting */
         this->buckets[index].value = NULL;
         this->items--;

         break;
      }

      if (this->buckets[index].probe < probe)
         break;

      index = (index + 1) % this->size;
      probe++;

      assert(index != origIndex);
   }

   assert(Hashtable_isConsistent(this));
   assert(Hashtable_get(this, key) == NULL);

   /* shrink on load-factor < 0.125 */
   if (8 * this->items < this->size)
      Hashtable_setSize(this, this->size / 2);

   return res;
}

void* Hashtable_get(Hashtable* this, hkey_t key) {
   unsigned int index = key % this->size;
   unsigned int probe = 0;
   void* res = NULL;
#ifndef NDEBUG
   unsigned int origIndex = index;
#endif

   assert(Hashtable_isConsistent(this));

   while (this->buckets[index].value) {
      if (this->buckets[index].key == key) {
         res = this->buckets[index].value;
         break;
      }

      if (this->buckets[index].probe < probe)
         break;

      index = (index + 1) != this->size ? (index + 1) : 0;
      probe++;

      assert(index != origIndex);
   }

   return res;
}

void Hashtable_foreach(Hashtable* this, Hashtable_PairFunction f, void* userData) {
   assert(Hashtable_isConsistent(this));
   for (unsigned int i = 0; i < this->size; i++) {
      HashtableItem* walk = &this->buckets[i];
      if (walk->value)
         f(walk->key, walk->value, userData);
   }
   assert(Hashtable_isConsistent(this));
}
