 /*
htop - generic/sort.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/sort.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h> // IWYU pragma: keep

typedef struct MergeSortContext_ {
   size_t elementSize;
   Object_Compare compare;
   void* compareContext;
   char* array;
} MergeSortContext;

static void swapByte(char* p1, char* p2) {
   char temp = *p1;
   *p1 = *p2;
   *p2 = temp;
}

static void mergeRuns(MergeSortContext* mctx, void* start, void* mid, const void* end) {
   size_t* size = &mctx->elementSize;

   assert(*size > 0);
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

   char* p1 = (char*)mid;
   do {
      p1 -= 1;
      swapByte(p1, p1 + rotateSize);
   } while (p1 + rotateSize > (char*)mid);

   size_t rightSize = (size_t)((const char*)end - (char*)mid);

   char* newStart;
   char* newMid;
   const char* newEnd;
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
   mergeRuns(mctx, start, mid, end);
}

static void* mergeSortSubarray(MergeSortContext* mctx, void* windowEnd, void* windowStart) {
   size_t* size = &mctx->elementSize;
   char** array = &mctx->array;

   assert(*size > 0);
   assert((char*)windowStart >= *array);
   assert((size_t)((char*)windowStart - *array) % *size == 0);

   // A run is a sorted subarray. Each recursive call of this function keeps
   // the lengths of two runs. At most O(log(n)) lengths of runs will be
   // tracked on the call stack.
   char* runStart0 = (char*)windowEnd;
   while (runStart0 > (char*)windowStart) {
      assert((size_t)((char*)windowEnd - (char*)windowStart) % *size == 0);

      char* runStart2 = runStart0;

      while (true) {
         if (runStart0 <= (char*)windowStart)
            return windowEnd;

         runStart0 -= *size;

         if (runStart0 <= *array)
            break;

         const char* p1 = runStart0 - *size;
         if (mctx->compare(p1, runStart0, mctx->compareContext) > 0) {
            break;
         }
      }

      assert(runStart2 <= (char*)windowEnd);
      if (runStart2 >= (char*)windowEnd)
         continue;

      char* runStart1 = runStart0;

      char* nextWindow = windowStart;
      size_t runSize2 = (size_t)((char*)windowEnd - runStart2);
      // This comparison is safe against negative overflow.
      if ((size_t)(runStart1 - (char*)windowStart) > runSize2) {
         nextWindow = runStart1 - runSize2;
         assert(nextWindow > (char*)windowStart);
      }

      runStart0 = mergeSortSubarray(mctx, runStart1, nextWindow);
      assert(runStart0 <= runStart1);

      mergeRuns(mctx, runStart0, runStart1, runStart2);
      mergeRuns(mctx, runStart0, runStart2, windowEnd);

      if (nextWindow == (char*)windowStart) {
         break;
      }
   }
   return runStart0;
}

void Generic_sort(void* array, size_t len, size_t size, Object_Compare compare, void* context) {
   if (!size)
      return;

   assert(len <= SIZE_MAX / size);

   MergeSortContext mctx = {
      .elementSize = size,
      .compare = compare,
      .compareContext = context,
      .array = array
   };
   (void)mergeSortSubarray(&mctx, (char*)array + len * size, array);
}
