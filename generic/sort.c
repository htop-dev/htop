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


static void swapByte(char* p1, char* p2) {
    char temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

static void rotate(void* buffer, size_t leftSize, size_t rightSize) {
   if (rightSize == 0)
      return;

   char* p1 = buffer;
   char* p2 = p1 + leftSize;
   char* mid = p2;
   const char* const end = mid + rightSize;

   while (true) {
      // Ensure there is no arithmetic overflow on input.
      assert(p1 <= mid);
      assert(mid <= p2);
      assert(p2 <= end);

      if (p2 >= end) {
         assert(mid < end);
         p2 = mid;
      }

      if (p1 >= p2)
         break;

      if (p1 >= mid)
         mid = p2;

      swapByte(p1, p2);
      p1 += 1;
      p2 += 1;
   }
}

static void mergeRuns(void* array, size_t leftLen, size_t rightLen, size_t size, Object_Compare compare, void* context) {
   assert(size > 0);
   if (leftLen == 0 || rightLen == 0 || size == 0)
      return;

   assert(leftLen <= SIZE_MAX / size);
   assert(rightLen <= SIZE_MAX / size);

   char* p1 = array;
   char* p2 = p1 + leftLen * size;
   char* mid = p2;
   const char* const end = mid + rightLen * size;

   for (size_t limit = (leftLen + rightLen) / 2; limit > 0; limit--) {
      // Ensure there is no arithmetic overflow on input.
      assert(p1 <= mid);
      assert(mid <= p2);
      assert(p2 <= end);

      if (p1 >= mid || p2 >= end)
         break;

      if (compare(p1, p2, context) <= 0) {
         p1 += size;
      } else {
         p2 += size;
      }
   }

   rotate(p1, (size_t)(mid - p1), (size_t)(p2 - mid));

   leftLen = (size_t)(p1 - (char*)array) / size;
   rightLen = (size_t)(p2 - mid) / size;
   mergeRuns(array, leftLen, rightLen, size, compare, context);

   leftLen = (size_t)(mid - p1) / size;
   rightLen = (size_t)(end - p2) / size;
   mergeRuns(p1 + (p2 - mid), leftLen, rightLen, size, compare, context);
}

static size_t mergeSortSubarray(void* array, size_t unsortedLen, size_t limit, size_t size, Object_Compare compare, void* context) {
   assert(size > 0);
   if (size == 0)
      return 0;

   // The initial level of this function call must set "limit" to 0. Subsequent
   // levels of recursion will have "limit" no less than the previous level.

   // A run is a sorted subarray. Each recursive call of this function keeps
   // the lengths of two runs. At most O(log(n)) lengths of runs will be
   // tracked on the call stack.
   size_t runLen[3] = {0};
   while (unsortedLen > 0) {
      size_t totalLen = unsortedLen;
      assert(totalLen <= SIZE_MAX / size);
      while (true) {
         --unsortedLen;

         const char* p2 = (const char*)array + unsortedLen * size;
         // Ensure there is no arithmetic overflow on input.
         assert(p2 >= (const char*)array);

         if (unsortedLen < limit)
            return 0;

         if (unsortedLen == 0 || compare(p2 - 1 * size, p2, context) > 0) {
            break;
         }
      }
      runLen[1] = totalLen - unsortedLen;

      bool reachesLimit = false;

      assert(runLen[2] > 0 || runLen[0] == 0);
      if (runLen[2] > 0) {
         size_t nextLimit = limit;
         if (unsortedLen > runLen[2] + limit) {
            nextLimit = unsortedLen - runLen[2];
         } else {
            reachesLimit = true;
         }

         runLen[0] = mergeSortSubarray(array, unsortedLen, nextLimit, size, compare, context);
         unsortedLen -= runLen[0];

         char* p1 = (char*)array + unsortedLen * size;
         mergeRuns(p1, runLen[0], runLen[1], size, compare, context);
         runLen[1] += runLen[0];
         runLen[0] = 0;

         mergeRuns(p1, runLen[1], runLen[2], size, compare, context);
      }
      runLen[2] += runLen[1];
      runLen[1] = 0;

      if (reachesLimit) {
         break;
      }
   }
   return runLen[2];
}

void Generic_sort(void* array, size_t len, size_t size, Object_Compare compare, void* context) {
   (void)mergeSortSubarray(array, len, 0, size, compare, context);
}
