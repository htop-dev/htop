/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Hashtable.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "XUtils.h"

#ifndef NDEBUG
#include <stdio.h>
#endif


typedef struct HashtableItem_ {
   ht_key_t key;
   size_t probe;
   void* value;
} HashtableItem;

struct Hashtable_ {
   size_t size;
   HashtableItem* buckets;
   size_t items;
   bool owner;
};


#ifndef NDEBUG

static void Hashtable_dump(const Hashtable* this) {
   fprintf(stderr, "Hashtable %p: size=%zu items=%zu owner=%s\n",
           (const void*)this,
           this->size,
           this->items,
           this->owner ? "yes" : "no");

   size_t items = 0;
   for (size_t i = 0; i < this->size; i++) {
      fprintf(stderr, "  item %5zu: key = %5u probe = %2zu value = %p\n",
              i,
              this->buckets[i].key,
              this->buckets[i].probe,
              this->buckets[i].value);

      if (this->buckets[i].value)
         items++;
   }

   fprintf(stderr, "Hashtable %p: items=%zu counted=%zu\n",
           (const void*)this,
           this->items,
           items);
}

static bool Hashtable_isConsistent(const Hashtable* this) {
   size_t items = 0;
   for (size_t i = 0; i < this->size; i++) {
      if (this->buckets[i].value)
         items++;
   }
   bool res = items == this->items;
   if (!res)
      Hashtable_dump(this);
   return res;
}

size_t Hashtable_count(const Hashtable* this) {
   size_t items = 0;
   for (size_t i = 0; i < this->size; i++) {
      if (this->buckets[i].value)
         items++;
   }
   assert(items == this->items);
   return items;
}

#endif /* NDEBUG */

/* https://oeis.org/A014234 */
static const uint64_t OEISprimes[] = {
   7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191,
   16381, 32749, 65521, 131071, 262139, 524287, 1048573,
   2097143, 4194301, 8388593, 16777213, 33554393,
   67108859, 134217689, 268435399, 536870909, 1073741789,
   2147483647, 4294967291, 8589934583, 17179869143,
   34359738337, 68719476731, 137438953447
};

static size_t nextPrime(size_t n) {
   /* on 32-bit make sure we do not return primes not fitting in size_t */
   for (size_t i = 0; i < ARRAYSIZE(OEISprimes) && OEISprimes[i] < SIZE_MAX; i++) {
      if (n <= OEISprimes[i])
         return OEISprimes[i];
   }

   CRT_fatalError("Hashtable: no prime found");
}

Hashtable* Hashtable_new(size_t size, bool owner) {
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
   Hashtable_clear(this);

   free(this->buckets);
   free(this);
}

void Hashtable_clear(Hashtable* this) {
   assert(Hashtable_isConsistent(this));

   if (this->owner)
      for (size_t i = 0; i < this->size; i++)
         free(this->buckets[i].value);

   memset(this->buckets, 0, this->size * sizeof(HashtableItem));
   this->items = 0;

   assert(Hashtable_isConsistent(this));
}

static void insert(Hashtable* this, ht_key_t key, void* value) {
   size_t index = key % this->size;
   size_t probe = 0;
#ifndef NDEBUG
   size_t origIndex = index;
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

void Hashtable_setSize(Hashtable* this, size_t size) {

   assert(Hashtable_isConsistent(this));

   if (size <= this->items)
      return;

   size_t newSize = nextPrime(size);
   if (newSize == this->size)
      return;

   HashtableItem* oldBuckets = this->buckets;
   size_t oldSize = this->size;

   this->size = newSize;
   this->buckets = (HashtableItem*) xCalloc(this->size, sizeof(HashtableItem));
   this->items = 0;

   /* rehash */
   for (size_t i = 0; i < oldSize; i++) {
      if (!oldBuckets[i].value)
         continue;

      insert(this, oldBuckets[i].key, oldBuckets[i].value);
   }

   free(oldBuckets);

   assert(Hashtable_isConsistent(this));
}

void Hashtable_put(Hashtable* this, ht_key_t key, void* value) {

   assert(Hashtable_isConsistent(this));
   assert(this->size > 0);
   assert(value);

   /* grow on load-factor > 0.7 */
   if (10 * this->items > 7 * this->size) {
      if (SIZE_MAX / 2 < this->size)
         CRT_fatalError("Hashtable: size overflow");

      Hashtable_setSize(this, 2 * this->size);
   }

   insert(this, key, value);

   assert(Hashtable_isConsistent(this));
   assert(Hashtable_get(this, key) != NULL);
   assert(this->size > this->items);
}

void* Hashtable_remove(Hashtable* this, ht_key_t key) {
   size_t index = key % this->size;
   size_t probe = 0;
#ifndef NDEBUG
   size_t origIndex = index;
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

         size_t next = (index + 1) % this->size;

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
      Hashtable_setSize(this, this->size / 3); /* account for nextPrime rounding up */

   return res;
}

void* Hashtable_get(Hashtable* this, ht_key_t key) {
   size_t index = key % this->size;
   size_t probe = 0;
   void* res = NULL;
#ifndef NDEBUG
   size_t origIndex = index;
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
   for (size_t i = 0; i < this->size; i++) {
      HashtableItem* walk = &this->buckets[i];
      if (walk->value)
         f(walk->key, walk->value, userData);
   }
   assert(Hashtable_isConsistent(this));
}
