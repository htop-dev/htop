
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <assert.h>

#include "DebugMemory.h"

#undef strdup
#undef malloc
#undef realloc
#undef calloc
#undef free

/*{
typedef struct DebugMemoryItem_ DebugMemoryItem;

struct DebugMemoryItem_ {
   void* data;
   char* file;
   int line;
   DebugMemoryItem* next;
};

typedef struct DebugMemory_ {
   DebugMemoryItem* first;
   int allocations;
   int deallocations;
   int size;
   FILE* file;
} DebugMemory;
}*/

/* private property */
DebugMemory* singleton = NULL;

void DebugMemory_new() {
   if (singleton)
      return;
   singleton = malloc(sizeof(DebugMemory));
   singleton->first = NULL;
   singleton->allocations = 0;
   singleton->deallocations = 0;
   singleton->size = 0;
   singleton->file = fopen("/tmp/htop-debug-alloc.txt", "w");
}

void* DebugMemory_malloc(int size, char* file, int line) {
   void* data = malloc(size);
   DebugMemory_registerAllocation(data, file, line);
   fprintf(singleton->file, "%d\t%s:%d\n", size, file, line);
   return data;
}

void* DebugMemory_calloc(int a, int b, char* file, int line) {
   void* data = calloc(a, b);
   DebugMemory_registerAllocation(data, file, line);
   fprintf(singleton->file, "%d\t%s:%d\n", a*b, file, line);
   return data;
}

void* DebugMemory_realloc(void* ptr, int size, char* file, int line) {
   if (ptr != NULL)
      DebugMemory_registerDeallocation(ptr, file, line);
   void* data = realloc(ptr, size);
   DebugMemory_registerAllocation(data, file, line);
   fprintf(singleton->file, "%d\t%s:%d\n", size, file, line);
   return data;
}

void* DebugMemory_strdup(char* str, char* file, int line) {
   char* data = strdup(str);
   DebugMemory_registerAllocation(data, file, line);
   fprintf(singleton->file, "%d\t%s:%d\n", (int) strlen(str), file, line);
   return data;
}

void DebugMemory_free(void* data, char* file, int line) {
   DebugMemory_registerDeallocation(data, file, line);
   free(data);
}

void DebugMemory_assertSize() {
   if (!singleton->first) {
      assert (singleton->size == 0);
   }
   DebugMemoryItem* walk = singleton->first;
   int i = 0;
   while (walk != NULL) {
      i++;
      walk = walk->next;
   }
   assert (i == singleton->size);
}

int DebugMemory_getBlockCount() {
   if (!singleton->first) {
      return 0;
   }
   DebugMemoryItem* walk = singleton->first;
   int i = 0;
   while (walk != NULL) {
      i++;
      walk = walk->next;
   }
   return i;
}

void DebugMemory_registerAllocation(void* data, char* file, int line) {
   if (!singleton)
      DebugMemory_new();
   DebugMemory_assertSize();
   DebugMemoryItem* item = (DebugMemoryItem*) malloc(sizeof(DebugMemoryItem));
   item->data = data;
   item->file = file;
   item->line = line;
   item->next = NULL;
   int val = DebugMemory_getBlockCount();
   if (singleton->first == NULL) {
      assert (val == 0);
      singleton->first = item;
   } else {
      DebugMemoryItem* walk = singleton->first;
      while (true) {
         if (walk->next == NULL) {
            walk->next = item;
            break;
         }
         walk = walk->next;
      }
   }
   int nval = DebugMemory_getBlockCount();
   assert(nval == val + 1);
   singleton->allocations++;
   singleton->size++;
   DebugMemory_assertSize();
}

void DebugMemory_registerDeallocation(void* data, char* file, int line) {
   assert(singleton);
   assert(singleton->first);
   DebugMemoryItem* walk = singleton->first;
   DebugMemoryItem* prev = NULL;
   int val = DebugMemory_getBlockCount();
   while (walk != NULL) {
      if (walk->data == data) {
         if (prev == NULL) {
            singleton->first = walk->next;
         } else {
            prev->next = walk->next;
         }
         free(walk);
         assert(DebugMemory_getBlockCount() == val - 1);
         singleton->deallocations++;
         singleton->size--;
         DebugMemory_assertSize();
         return;
      }
      DebugMemoryItem* tmp = walk;
      walk = walk->next;
      prev = tmp;
   }
   DebugMemory_report();
   fprintf(stderr, "Couldn't find allocation for memory freed at %s:%d\n", file, line);
   assert(false);
}

void DebugMemory_report() {
   assert(singleton);
   DebugMemoryItem* walk = singleton->first;
   int i = 0;
   while (walk != NULL) {
      i++;
      fprintf(stderr, "%p %s:%d\n", walk->data, walk->file, walk->line);
      walk = walk->next;
   }
   fprintf(stderr, "Total:\n");
   fprintf(stderr, "%d allocations\n", singleton->allocations);
   fprintf(stderr, "%d deallocations\n", singleton->deallocations);
   fprintf(stderr, "%d size\n", singleton->size);
   fprintf(stderr, "%d non-freed blocks\n", i);
   fclose(singleton->file);
}
