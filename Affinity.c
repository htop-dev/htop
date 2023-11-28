/*
htop - Affinity.c
(C) 2004-2011 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Affinity.h"

#include <assert.h>
#include <stdlib.h>

#include "Process.h"
#include "XUtils.h"

#if defined(HAVE_LIBHWLOC)
#include <hwloc.h>
#include <hwloc/bitmap.h>
#ifdef __linux__
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_THREAD
#else
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_PROCESS
#endif
#elif defined(HAVE_AFFINITY)
#include <sched.h>
#endif


Affinity* Affinity_new(Machine* host) {
   Affinity* this = xCalloc(1, sizeof(Affinity));
   this->size = 8;
   this->cpus = xCalloc(this->size, sizeof(unsigned int));
   this->host = host;
   return this;
}

void Affinity_delete(Affinity* this) {
   free(this->cpus);
   free(this);
}

void Affinity_add(Affinity* this, unsigned int id) {
   if (this->used == this->size) {
      this->size *= 2;
      this->cpus = xRealloc(this->cpus, sizeof(unsigned int) * this->size);
   }
   this->cpus[this->used] = id;
   this->used++;
}

#if defined(HAVE_LIBHWLOC)

static Affinity* Affinity_get(const Process* p, Machine* host) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   bool ok = (hwloc_get_proc_cpubind(host->topology, Process_getPid(p), cpuset, HTOP_HWLOC_CPUBIND_FLAG) == 0);
   Affinity* affinity = NULL;
   if (ok) {
      affinity = Affinity_new(host);
      if (hwloc_bitmap_last(cpuset) == -1) {
         for (unsigned int i = 0; i < host->existingCPUs; i++) {
            Affinity_add(affinity, i);
         }
      } else {
         int id;
         hwloc_bitmap_foreach_begin(id, cpuset)
            Affinity_add(affinity, (unsigned)id);
         hwloc_bitmap_foreach_end();
      }
   }
   hwloc_bitmap_free(cpuset);
   return affinity;
}

static bool Affinity_set(Process* p, Arg arg) {
   Affinity* this = arg.v;
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   for (unsigned int i = 0; i < this->used; i++) {
      hwloc_bitmap_set(cpuset, this->cpus[i]);
   }
   bool ok = (hwloc_set_proc_cpubind(this->host->topology, Process_getPid(p), cpuset, HTOP_HWLOC_CPUBIND_FLAG) == 0);
   hwloc_bitmap_free(cpuset);
   return ok;
}

#elif defined(HAVE_AFFINITY)

static Affinity* Affinity_get(const Process* p, Machine* host) {
   cpu_set_t cpuset;
   bool ok = (sched_getaffinity(Process_getPid(p), sizeof(cpu_set_t), &cpuset) == 0);
   if (!ok)
      return NULL;

   Affinity* affinity = Affinity_new(host);
   for (unsigned int i = 0; i < host->existingCPUs; i++) {
      if (CPU_ISSET(i, &cpuset)) {
         Affinity_add(affinity, i);
      }
   }
   return affinity;
}

static bool Affinity_set(Process* p, Arg arg) {
   Affinity* this = arg.v;
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   for (unsigned int i = 0; i < this->used; i++) {
      CPU_SET(this->cpus[i], &cpuset);
   }
   bool ok = (sched_setaffinity(Process_getPid(p), sizeof(unsigned long), &cpuset) == 0);
   return ok;
}

#endif

#if defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY)

bool Affinity_rowSet(Row* row, Arg arg) {
   Process* p = (Process*) row;
   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   return Affinity_set(p, arg);
}

Affinity* Affinity_rowGet(const Row* row, Machine* host) {
   const Process* p = (const Process*) row;
   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   return Affinity_get(p, host);
}

#endif /* HAVE_LIBHWLOC || HAVE_AFFINITY */
