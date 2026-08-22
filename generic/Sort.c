/*
htop - generic/Sort.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/Sort.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h> // IWYU pragma: keep

typedef struct MergeSortContext_ {
   /* To minimize code size, the most frequently referenced member of this
      structure is ordered first. */
   size_t elementSize;
   Object_Compare compare;
   void* compareContext;
   void* array;
} MergeSortContext;

static void swapByte(char* p1, char* p2) {
   char temp = *p1;
   *p1 = *p2;
   *p2 = temp;
}

static void mergeRuns(MergeSortContext* mctx, void* start, void* mid, const void* end) {
   size_t* size = &mctx->elementSize;

   assert(*size > 0);

   // This is a stable merge using rotation. Idea come from Xinok, see:
   // https://xinok.wordpress.com/2014/08/17/in-place-merge-sort-demystified-2/
   while (true) {
      assert(mid < start || (size_t)((char*)mid - (char*)start) % *size == 0);
      assert(end < mid || (size_t)((const char*)end - (char*)mid) % *size == 0);

      size_t rotateSize = 0;
      while (true) {
         char* p2 = (char*)mid + rotateSize;
         if (p2 >= (const char*)end)
            break;

         char* p1 = (char*)mid - rotateSize;
         if (p1 <= (char*)start)
            break;
         p1 -= *size;

         if (mctx->compare(p1, p2, mctx->compareContext) <= 0)
            break;

         rotateSize += *size;
      }

      if (rotateSize == 0)
         return;

      char* p1 = mid;
      do {
         p1 -= 1;
         swapByte(p1, p1 + rotateSize);
      } while (p1 + rotateSize > (char*)mid);

      size_t rightSize = (size_t)((const char*)end - (char*)mid);

      void* newStart;
      void* newMid;
      const void* newEnd;
      if ((char*)mid <= (char*)start + rightSize) {
         newStart = start;
         newMid = (char*)mid - rotateSize;
         newEnd = mid;
         start = mid;
         mid = (char*)mid + rotateSize;
      } else {
         newStart = mid;
         newMid = (char*)mid + rotateSize;
         newEnd = end;
         end = mid;
         mid = (char*)mid - rotateSize;
      }
      mergeRuns(mctx, newStart, newMid, newEnd);
   }
}

static void* mergeSortSubarray(MergeSortContext* mctx, size_t nextWindowSize, void* windowStart, void* windowEnd) {
   size_t* size = &mctx->elementSize;
   void** array = &mctx->array;

   assert(*size > 0);
   assert(windowStart >= *array);
   assert((size_t)((char*)windowStart - (char*)*array) % *size == 0);
   assert(windowEnd >= windowStart);
   assert((size_t)((char*)windowEnd - (char*)windowStart) % *size == 0);

   // A run is a sorted subarray. Each recursive call of this function records
   // the length of one run. At most O(log(n)) lengths of runs are tracked on
   // the call stack.
   char* newRun = (char*)windowEnd;
   while (true) {
      if (newRun <= (char*)windowStart)
         return windowEnd;

      newRun -= *size;

      if (newRun <= (char*)*array)
         return newRun;

      if (mctx->compare(newRun - *size, newRun, mctx->compareContext) > 0) {
         break;
      }
   }

   while (true) {
      assert(nextWindowSize > 0);
      assert(nextWindowSize % *size == 0);

      char* nextWindow = windowStart;
      if (newRun > (char*)windowStart + nextWindowSize) {
         // This avoids call-preserving "nextWindowSize" (reduces code size).
         nextWindow += (size_t)(newRun - ((char*)windowStart + nextWindowSize));
         assert(nextWindow == newRun - nextWindowSize);
         assert(nextWindow > (char*)windowStart);
      }

      char* lastRun = newRun;
      size_t lastRunSize = (size_t)((char*)windowEnd - lastRun);
      newRun = mergeSortSubarray(mctx, lastRunSize, nextWindow, lastRun);
      assert(newRun <= lastRun);

      if (newRun >= lastRun) {
         if (newRun <= (char*)windowStart + nextWindowSize && windowStart > *array)
            return windowEnd;
         break;
      }

      mergeRuns(mctx, newRun, lastRun, windowEnd);

      if (newRun <= (char*)windowStart + lastRunSize) {
         break;
      }
   }
   return newRun;
}

static void mergeSort(void* array, size_t len, size_t size, Object_Compare compare, void* context) {
   if (!size)
      return;

   assert(len <= SIZE_MAX / size);

   MergeSortContext mctx = {
      .elementSize = size,
      .compare = compare,
      .compareContext = context,
      .array = array
   };
   (void)mergeSortSubarray(&mctx, len * size, array, (char*)array + len * size);
}

void Sort_sort(void* array, size_t len, size_t size, Object_Compare compare, void* context) {
   mergeSort(array, len, size, compare, context);
}
