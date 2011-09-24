/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009-2010 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/* Detect topology change: registering for power management changes and check
 * if for example hw.activecpu changed */

/* Apparently, Darwin people do not _want_ to provide binding functions.  */

#include <private/autogen/config.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <inttypes.h>

#include <hwloc.h>
#include <private/private.h>
#include <private/debug.h>

void
hwloc_look_darwin(struct hwloc_topology *topology)
{
  int64_t _nprocs;
  unsigned nprocs;
  int64_t _npackages;
  unsigned i, j, cpu;
  struct hwloc_obj *obj;
  size_t size;
  int64_t l1cachesize;
  int64_t l2cachesize;
  int64_t cachelinesize;
  int64_t memsize;

  if (hwloc_get_sysctlbyname("hw.ncpu", &_nprocs) || _nprocs <= 0)
    return;
  nprocs = _nprocs;
  topology->support.discovery->pu = 1;

  hwloc_debug("%u procs\n", nprocs);

  if (!hwloc_get_sysctlbyname("hw.packages", &_npackages) && _npackages > 0) {
    unsigned npackages = _npackages;
    int64_t _cores_per_package;
    int64_t _logical_per_package;
    unsigned logical_per_package;

    hwloc_debug("%u packages\n", npackages);

    if (!hwloc_get_sysctlbyname("machdep.cpu.logical_per_package", &_logical_per_package) && _logical_per_package > 0)
      logical_per_package = _logical_per_package;
    else
      /* Assume the trivia.  */
      logical_per_package = nprocs / npackages;

    hwloc_debug("%u threads per package\n", logical_per_package);


    if (nprocs == npackages * logical_per_package)
      for (i = 0; i < npackages; i++) {
        obj = hwloc_alloc_setup_object(HWLOC_OBJ_SOCKET, i);
        obj->cpuset = hwloc_bitmap_alloc();
        for (cpu = i*logical_per_package; cpu < (i+1)*logical_per_package; cpu++)
          hwloc_bitmap_set(obj->cpuset, cpu);

        hwloc_debug_1arg_bitmap("package %u has cpuset %s\n",
                   i, obj->cpuset);
        hwloc_insert_object_by_cpuset(topology, obj);
      }

    if (!hwloc_get_sysctlbyname("machdep.cpu.cores_per_package", &_cores_per_package) && _cores_per_package > 0) {
      unsigned cores_per_package = _cores_per_package;
      hwloc_debug("%u cores per package\n", cores_per_package);

      if (!(logical_per_package % cores_per_package))
        for (i = 0; i < npackages * cores_per_package; i++) {
          obj = hwloc_alloc_setup_object(HWLOC_OBJ_CORE, i);
          obj->cpuset = hwloc_bitmap_alloc();
          for (cpu = i*(logical_per_package/cores_per_package);
               cpu < (i+1)*(logical_per_package/cores_per_package);
               cpu++)
            hwloc_bitmap_set(obj->cpuset, cpu);

          hwloc_debug_1arg_bitmap("core %u has cpuset %s\n",
                     i, obj->cpuset);
          hwloc_insert_object_by_cpuset(topology, obj);
        }
    }
  }

  if (hwloc_get_sysctlbyname("hw.l1dcachesize", &l1cachesize))
    l1cachesize = 0;

  if (hwloc_get_sysctlbyname("hw.l2cachesize", &l2cachesize))
    l2cachesize = 0;

  if (hwloc_get_sysctlbyname("hw.cachelinesize", &cachelinesize))
    cachelinesize = 0;

  if (hwloc_get_sysctlbyname("hw.memsize", &memsize))
    memsize = 0;

  if (!sysctlbyname("hw.cacheconfig", NULL, &size, NULL, 0)) {
    unsigned n = size / sizeof(uint32_t);
    uint64_t *cacheconfig = NULL;
    uint64_t *cachesize = NULL;
    uint32_t *cacheconfig32 = NULL;

    cacheconfig = malloc(sizeof(uint64_t) * n);
    if (NULL == cacheconfig) {
        goto out;
    }
    cachesize = malloc(sizeof(uint64_t) * n);
    if (NULL == cachesize) {
        goto out;
    }
    cacheconfig32 = malloc(sizeof(uint32_t) * n);
    if (NULL == cacheconfig32) {
        goto out;
    }

    if ((!sysctlbyname("hw.cacheconfig", cacheconfig, &size, NULL, 0))) {
      /* Yeech. Darwin seemingly has changed from 32bit to 64bit integers for
       * cacheconfig, with apparently no way for detection. Assume the machine
       * won't have more than 4 billion cpus */
      if (cacheconfig[0] > 0xFFFFFFFFUL) {
        memcpy(cacheconfig32, cacheconfig, size);
        for (i = 0 ; i < size / sizeof(uint32_t); i++)
          cacheconfig[i] = cacheconfig32[i];
      }

      memset(cachesize, 0, sizeof(uint64_t) * n);
      size = sizeof(uint64_t) * n;
      if (sysctlbyname("hw.cachesize", cachesize, &size, NULL, 0)) {
        if (n > 0)
          cachesize[0] = memsize;
        if (n > 1)
          cachesize[1] = l1cachesize;
        if (n > 2)
          cachesize[2] = l2cachesize;
      }

      hwloc_debug("%s", "caches");
      for (i = 0; i < n && cacheconfig[i]; i++)
        hwloc_debug(" %"PRIu64"(%"PRIu64"kB)", cacheconfig[i], cachesize[i] / 1024);

      cacheconfig[i] = cacheconfig32[i];
      /* Now we know how many caches there are */
      n = i;
      hwloc_debug("\n%u cache levels\n", n - 1);

      /* For each cache level (0 is memory) */
      for (i = 0; i < n; i++) {
        /* cacheconfig tells us how many cpus share it, let's iterate on each cache */
        for (j = 0; j < (nprocs / cacheconfig[i]); j++) {
          obj = hwloc_alloc_setup_object(i?HWLOC_OBJ_CACHE:HWLOC_OBJ_NODE, j);
          if (!i) {
            obj->nodeset = hwloc_bitmap_alloc();
            hwloc_bitmap_set(obj->nodeset, j);
          }
          obj->cpuset = hwloc_bitmap_alloc();
          for (cpu = j*cacheconfig[i];
               cpu < ((j+1)*cacheconfig[i]);
               cpu++)
            hwloc_bitmap_set(obj->cpuset, cpu);

          if (i) {
            hwloc_debug_2args_bitmap("L%ucache %u has cpuset %s\n",
                i, j, obj->cpuset);
            obj->attr->cache.depth = i;
            obj->attr->cache.size = cachesize[i];
            obj->attr->cache.linesize = cachelinesize;
          } else {
            hwloc_debug_1arg_bitmap("node %u has cpuset %s\n",
                j, obj->cpuset);
	    obj->memory.local_memory = cachesize[i];
	    obj->memory.page_types_len = 2;
	    obj->memory.page_types = malloc(2*sizeof(*obj->memory.page_types));
	    memset(obj->memory.page_types, 0, 2*sizeof(*obj->memory.page_types));
	    obj->memory.page_types[0].size = getpagesize();
#ifdef HAVE__SC_LARGE_PAGESIZE
	    obj->memory.page_types[1].size = sysconf(_SC_LARGE_PAGESIZE);
#endif
          }

          hwloc_insert_object_by_cpuset(topology, obj);
        }
      }
    }
  out:
    if (NULL != cacheconfig) {
        free(cacheconfig);
    }
    if (NULL != cachesize) {
        free(cachesize);
    }
    if (NULL != cacheconfig32) {
        free(cacheconfig32);
    }
  }


  /* add PU objects */
  hwloc_setup_pu_level(topology, nprocs);

  hwloc_add_object_info(topology->levels[0][0], "Backend", "Darwin");
}

void
hwloc_set_darwin_hooks(struct hwloc_topology *topology __hwloc_attribute_unused)
{
}
