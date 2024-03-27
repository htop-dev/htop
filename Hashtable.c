/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Hashtable.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
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

#define MIN_TABLE_SIZE 11

/* Primes borrowed from gnulib/lib/gl_anyhash_primes.h.

   Array of primes, approximately in steps of factor 1.2.
   This table was computed by executing the Common Lisp expression
     (dotimes (i 244) (format t "nextprime(~D)~%" (ceiling (expt 1.2d0 i))))
   and feeding the result to PARI/gp. */
static const size_t primes[] = {
   MIN_TABLE_SIZE, 13, 17, 19, 23, 29, 37, 41, 47, 59, 67, 83, 97, 127, 139,
   167, 199, 239, 293, 347, 419, 499, 593, 709, 853, 1021, 1229, 1471, 1777,
   2129, 2543, 3049, 3659, 4391, 5273, 6323, 7589, 9103, 10937, 13109, 15727,
   18899, 22651, 27179, 32609, 39133, 46957, 56359, 67619, 81157, 97369,
   116849, 140221, 168253, 201907, 242309, 290761, 348889, 418667, 502409,
   602887, 723467, 868151, 1041779, 1250141, 1500181, 1800191, 2160233,
   2592277, 3110741, 3732887, 4479463, 5375371, 6450413, 7740517, 9288589,
   11146307, 13375573, 16050689, 19260817, 23112977, 27735583, 33282701,
   39939233, 47927081, 57512503, 69014987, 82818011, 99381577, 119257891,
   143109469, 171731387, 206077643, 247293161, 296751781, 356102141, 427322587,
   512787097, 615344489, 738413383, 886096061, 1063315271, 1275978331,
   1531174013, 1837408799, 2204890543UL, 2645868653UL, 3175042391UL,
   3810050851UL,
   /* on 32-bit make sure we do not return primes not fitting in size_t */
#if SIZE_MAX > 4294967295ULL
   4572061027ULL, 5486473229ULL, 6583767889ULL, 7900521449ULL, 9480625733ULL,
   /* Largest possible size should be 13652101063ULL == GROWTH_RATE((UINT32_MAX/3)*4)
      we include some larger values in case the above math is wrong */
   11376750877ULL, 13652101063ULL, 16382521261ULL, 19659025513ULL, 23590830631ULL,
#endif
};

static size_t nextPrime(size_t n) {
   for (size_t i = 0; i < ARRAYSIZE(primes); i++) {
      if (n < primes[i]) {
         return primes[i];
      }
   }

   CRT_fatalError("Hashtable: no prime found");
}

/* USABLE_FRACTION is the maximum hash map load.
 * Currently set to 2/3 capacity.
 *
 * Testing indicates that the median and average probe length
 * increases significantly after 2/3 of the hash map capacity.
 *
 * load = {size,capacity} / items
 *
 * | load | probe max | probe avg | probe median |
 * | ---- | --------- | --------- | ------------ |
 * | 0.60 |     90.58 |     20.19 |         0.65 |
 * | 0.65 |    167.00 |     37.07 |         1.58 |
 * | 0.70 |    230.54 |     61.40 |        15.54 |
 * | 0.75 |    287.00 |     85.23 |        26.15 |
 * | 0.80 |    287.00 |     94.71 |        55.93 |
 */
#define USABLE_FRACTION(n) (((n) << 1)/3)

/* SHRINK_THRESHOLD is number of items at with the hash map should shrink.
 * Currently set to 1/4 of the USABLE_FRACTION, which is ~13% of the total
 * hash map size.
 */
#define SHRINK_THRESHOLD(n) (USABLE_FRACTION((n)) / 4)

/* GROWTH_RATE. Growth rate upon hitting maximum load.
 * Currently set to items*3.
 * This means that hashes double in size when growing without deletions,
 * but have more head room when the number of deletions is on a par with the
 * number of insertions.
 */
#define GROWTH_RATE(h) ((h)->items*3)

static inline bool Hashtable_shouldResize(const Hashtable* this) {
   /* grow table */
   return this->items >= USABLE_FRACTION(this->size) ||
      /* shrink table */
      (this->size > MIN_TABLE_SIZE && this->items <= SHRINK_THRESHOLD(this->size));
}

Hashtable* Hashtable_new(size_t size, bool owner) {
   assert(MIN_TABLE_SIZE == primes[0]);
   Hashtable* this = xMalloc(sizeof(Hashtable));
   this->size = nextPrime(size);
   this->items = 0;
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

static inline size_t inc_index(size_t index, size_t size) {
   return ++index != size ? index : 0;
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

      index = inc_index(index, this->size);
      probe++;

      assert(index != origIndex);
   }
}

void Hashtable_setSize(Hashtable* this, size_t size) {

   assert(Hashtable_isConsistent(this));

   /* newSize will always be >= MIN_TABLE_SIZE */
   size_t newSize = nextPrime(size);
   if (newSize == this->size)
      return;

   assert(newSize > this->items);

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

   /* Resize the hash table, if necessary, before inserting */
   if (Hashtable_shouldResize(this)) {
      Hashtable_setSize(this, GROWTH_RATE(this));
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

         size_t next = inc_index(index, this->size);

         while (this->buckets[next].value && this->buckets[next].probe > 0) {
            this->buckets[index] = this->buckets[next];
            this->buckets[index].probe -= 1;

            index = next;
            next = inc_index(index, this->size);
         }

         /* set empty after backward shifting */
         this->buckets[index].value = NULL;
         this->items--;

         break;
      }

      if (this->buckets[index].probe < probe)
         break;

      index = inc_index(index, this->size);
      probe++;

      assert(index != origIndex);
   }

   assert(Hashtable_isConsistent(this));
   assert(Hashtable_get(this, key) == NULL);

   return res;
}

void* Hashtable_get(const Hashtable* this, ht_key_t key) {
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

      index = inc_index(index, this->size);
      probe++;

      assert(index != origIndex);
   }

   return res;
}

void Hashtable_foreach(const Hashtable* this, Hashtable_PairFunction f, void* userData) {
   assert(Hashtable_isConsistent(this));
   for (size_t i = 0; i < this->size; i++) {
      HashtableItem* walk = &this->buckets[i];
      if (walk->value)
         f(walk->key, walk->value, userData);
   }
   assert(Hashtable_isConsistent(this));
}
