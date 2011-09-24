
#include "config.h"
#include "Affinity.h"
#include <stdlib.h>

/*{

typedef struct Affinity_ {
   int size;
   int used;
   int* cpus;
} Affinity;

}*/

Affinity* Affinity_new() {
   Affinity* this = calloc(sizeof(Affinity), 1);
   this->size = 8;
   this->cpus = calloc(sizeof(int), this->size);
   return this;
}

void Affinity_delete(Affinity* this) {
   free(this->cpus);
   free(this);
}

void Affinity_add(Affinity* this, int id) {
   if (this->used == this->size) {
      this->size *= 2;
      this->cpus = realloc(this->cpus, sizeof(int) * this->size);
   }
   this->cpus[this->used] = id;
   this->used++;
}

