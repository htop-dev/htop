#include "config.h" // IWYU pragma: keep

#include "GenericHashtable.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "CRT.h"
#include "Macros.h"


typedef struct GenericHashtableItem_ {
   size_t probe;
   ght_data_t value;
} GenericHashtableItem;

struct GenericHashtable_ {
   size_t size;
   GenericHashtableItem* buckets;
   size_t items;
   bool owner;
   GenericHashtable_HashFunction hash;
   GenericHashtable_CompareFunction compare;
   GenericHashtable_DestroyFunction destroy;
};


#ifndef NDEBUG

static void GenericHashtable_dump(const GenericHashtable* this) {
   fprintf(stderr, "GenericHashtable %p: size=%zu items=%zu owner=%s\n",
           (const void*)this,
           this->size,
           this->items,
           this->owner ? "yes" : "no");

   size_t items = 0;
   for (size_t i = 0; i < this->size; i++) {
      fprintf(stderr, "  item %5zu: probe = %2zu value = %p\n",
              i,
              this->buckets[i].probe,
              this->buckets[i].value ? (const void*)this->buckets[i].value : "(nil)");

      if (this->buckets[i].value)
         items++;
   }

   fprintf(stderr, "GenericHashtable %p: items=%zu counted=%zu\n",
           (const void*)this,
           this->items,
           items);
}

static bool GenericHashtable_isConsistent(const GenericHashtable* this) {
   size_t items = 0;
   for (size_t i = 0; i < this->size; i++) {
      if (this->buckets[i].value)
         items++;
   }
   bool res = items == this->items;
   if (!res)
      GenericHashtable_dump(this);
   return res;
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

static size_t nextPrime(size_t n) {
   /* on 32-bit make sure we do not return primes not fitting in size_t */
   for (size_t i = 0; i < ARRAYSIZE(OEISprimes) && OEISprimes[i] < SIZE_MAX; i++) {
      if (n <= OEISprimes[i])
         return OEISprimes[i];
   }

   CRT_fatalError("GenericHashtable: no prime found");
}

GenericHashtable* GenericHashtable_new(
      size_t size,
      bool owner,
      GenericHashtable_HashFunction hash,
      GenericHashtable_CompareFunction compare,
      GenericHashtable_DestroyFunction destroy) {
   GenericHashtable* this;

   assert(hash);
   assert(compare);

   this = xMalloc(sizeof(GenericHashtable));
   this->items = 0;
   this->size = size ? nextPrime(size) : 13;
   this->buckets = (GenericHashtableItem*) xCalloc(this->size, sizeof(GenericHashtableItem));
   this->owner = owner;
   this->hash = hash;
   this->compare = compare;
   this->destroy = destroy;

   assert(GenericHashtable_isConsistent(this));
   return this;
}

void GenericHashtable_delete(GenericHashtable* this) {
   GenericHashtable_clear(this);

   free(this->buckets);
   free(this);
}

void GenericHashtable_clear(GenericHashtable* this) {
   assert(GenericHashtable_isConsistent(this));

   if (this->owner && this->destroy)
      for (size_t i = 0; i < this->size; i++)
         if (this->buckets[i].value)
            this->destroy(this->buckets[i].value);

   memset(this->buckets, 0, this->size * sizeof(GenericHashtableItem));
   this->items = 0;

   assert(GenericHashtable_isConsistent(this));
}

static void insert(GenericHashtable* this, ght_data_t value) {
   size_t index = this->hash(value) % this->size;
   size_t probe = 0;
   #ifndef NDEBUG
   size_t origIndex = index;
   #endif

   for (;;) {
      if (!this->buckets[index].value) {
         this->items++;
         this->buckets[index].probe = probe;
         this->buckets[index].value = value;
         return;
      }

      if (this->buckets[index].value == value || this->compare(this->buckets[index].value, value) == 0) {
         if (this->owner && this->destroy)
            this->destroy(this->buckets[index].value);
         this->buckets[index].value = value;
         return;
      }

      /* Robin Hood swap */
      if (probe > this->buckets[index].probe) {
         GenericHashtableItem tmp = this->buckets[index];

         this->buckets[index].probe = probe;
         this->buckets[index].value = value;

         probe = tmp.probe;
         value = tmp.value;
      }

      index = (index + 1) % this->size;
      probe++;

      assert(index != origIndex);
   }
}

void GenericHashtable_setSize(GenericHashtable* this, size_t size) {

   assert(GenericHashtable_isConsistent(this));

   if (size <= this->items)
      return;

   GenericHashtableItem* oldBuckets = this->buckets;
   size_t oldSize = this->size;

   this->size = nextPrime(size);
   this->buckets = (GenericHashtableItem*) xCalloc(this->size, sizeof(GenericHashtableItem));
   this->items = 0;

   /* rehash */
   for (size_t i = 0; i < oldSize; i++) {
      if (!oldBuckets[i].value)
         continue;

      insert(this, oldBuckets[i].value);
   }

   free(oldBuckets);

   assert(GenericHashtable_isConsistent(this));
}

ght_data_t GenericHashtable_get(GenericHashtable* this, ght_data_t value) {
   size_t index = this->hash(value) % this->size;
   size_t probe = 0;
   void* res = NULL;
   #ifndef NDEBUG
   size_t origIndex = index;
   #endif

   assert(GenericHashtable_isConsistent(this));

   while (this->buckets[index].value) {
      if (this->buckets[index].value == value || this->compare(this->buckets[index].value, value) == 0) {
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

void GenericHashtable_put(GenericHashtable* this, ght_data_t value) {

   assert(GenericHashtable_isConsistent(this));
   assert(this->size > 0);
   assert(value);

   /* grow on load-factor > 0.7 */
   if (10 * this->items > 7 * this->size) {
      if (SIZE_MAX / 2 < this->size)
         CRT_fatalError("GenericHashtable: size overflow");

      GenericHashtable_setSize(this, 2 * this->size);
   }

   insert(this, value);

   assert(GenericHashtable_isConsistent(this));
   assert(GenericHashtable_get(this, value) != NULL);
   assert(this->size > this->items);
}
