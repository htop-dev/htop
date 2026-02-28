/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Hashtable.h"

#include <assert.h>
#include <limits.h>
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

   assert(this->size > 0);
   assert(this->size <= SIZE_MAX / sizeof(HashtableItem));
   assert(this->size >= this->items);

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

static size_t nextPrime(size_t n) {
   // Table of differences so that (2^m - primeDiffs[m]) is a prime.
   // This is OEIS sequence https://oeis.org/A013603 except for
   // entry 0 (2^0 = 1 as a non-prime special case).
   static const uint8_t primeDiffs[] = {
      0, 0, 1, 1, 3, 1, 3, 1, 5, 3, 3, 9, 3, 1, 3, 19,
#if SIZE_MAX > UINT16_MAX
      15, 1, 5, 1, 3, 9, 3, 15, 3, 39, 5, 39, 57, 3, 35, 1,
#if SIZE_MAX > UINT32_MAX
      5, 9, 41, 31, 5, 25, 45, 7, 87, 21, 11, 57, 17, 55, 21, 115,
      59, 81, 27, 129, 47, 111, 33, 55, 5, 13, 27, 55, 93, 1, 57, 25,
#endif
#endif
   };

   assert(sizeof(n) * CHAR_BIT <= ARRAYSIZE(primeDiffs));
   for (uint8_t shift = 3; shift < sizeof(n) * CHAR_BIT; shift++) {
      size_t prime = ((size_t)1 << shift) - primeDiffs[shift];
      if (n <= prime) {
         return prime;
      }
   }

   CRT_fatalError("Hashtable: no prime found");
}

Hashtable* Hashtable_new(size_t size, bool owner) {
   size = size ? nextPrime(size) : 13;

   Hashtable* this = xMalloc(sizeof(Hashtable));
   *this = (Hashtable) {
      .items = 0,
      .size = size,
      .buckets = xCalloc(size, sizeof(HashtableItem)),
      .owner = owner,
   };

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

static void Hashtable_setSize(Hashtable* this, size_t size) {
   assert(Hashtable_isConsistent(this));
   assert(size >= this->items);

   size = nextPrime(size);
   if (size == this->size)
      return;

   HashtableItem* oldBuckets = this->buckets;
   size_t oldSize = this->size;

   this->size = size;
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
   assert(value);

   /* grow on load-factor > 0.7 */
   if (sizeof(HashtableItem) < 7 && SIZE_MAX / 7 < this->size)
      CRT_fatalError("Hashtable: size overflow");

   if (this->items >= this->size * 7 / 10)
      Hashtable_setSize(this, this->size + 1);

   insert(this, key, value);

   assert(Hashtable_isConsistent(this));
   assert(Hashtable_get(this, key) != NULL);
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
   if (sizeof(HashtableItem) < 3 && SIZE_MAX / 3 < this->size)
      CRT_fatalError("Hashtable: size overflow");

   if (this->items < this->size / 8) {
      Hashtable_setSize(this, this->size * 3 / 8); /* account for nextPrime rounding up */
   }

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
