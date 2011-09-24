/*
 * Copyright © 2010 INRIA.  All rights reserved.
 * Copyright © 2010-2011 Université Bordeaux 1
 * Copyright © 2010-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 *
 *
 * This backend is only used when the operating system does not export
 * the necessary hardware topology information to user-space applications.
 * Currently, only the FreeBSD backend relies on this x86 backend.
 *
 * Other backends such as Linux have their own way to retrieve various
 * pieces of hardware topology information from the operating system
 * on various architectures, without having to use this x86-specific code.
 */

#include <private/autogen/config.h>
#include <hwloc.h>
#include <private/private.h>
#include <private/debug.h>
#include <private/cpuid.h>
#include <private/misc.h>

struct cacheinfo {
  unsigned type;
  unsigned level;
  unsigned nbthreads_sharing;

  unsigned linesize;
  unsigned linepart;
  unsigned ways;
  unsigned sets;
  unsigned size;
};

struct procinfo {
  unsigned present;
  unsigned apicid;
  unsigned max_log_proc;
  unsigned max_nbcores;
  unsigned max_nbthreads;
  unsigned socketid;
  unsigned logprocid;
  unsigned threadid;
  unsigned coreid;
  unsigned *otherids;
  unsigned levels;
  unsigned numcaches;
  struct cacheinfo *cache;
};

enum cpuid_type {
  intel,
  amd,
  unknown
};

static void fill_amd_cache(struct procinfo *infos, unsigned level, unsigned cpuid)
{
  struct cacheinfo *cache;
  unsigned cachenum;
  unsigned size = 0;

  if (level == 1)
    size = ((cpuid >> 24)) << 10;
  else if (level == 2)
    size = ((cpuid >> 16)) << 10;
  else if (level == 3)
    size = ((cpuid >> 18)) << 19;
  if (!size)
    return;

  cachenum = infos->numcaches++;
  infos->cache = realloc(infos->cache, infos->numcaches*sizeof(*infos->cache));
  cache = &infos->cache[cachenum];

  cache->type = 1;
  cache->level = level;
  if (level <= 2)
    cache->nbthreads_sharing = 1;
  else
    cache->nbthreads_sharing = infos->max_log_proc;
  cache->linesize = cpuid & 0xff;
  cache->linepart = 0;
  if (level == 1)
    cache->ways = (cpuid >> 16) & 0xff;
  else {
    static const unsigned ways_tab[] = { 0, 1, 2, 0, 4, 0, 8, 0, 16, 0, 32, 48, 64, 96, 128, 0 };
    unsigned ways = (cpuid >> 12) & 0xf;
    cache->ways = ways_tab[ways];
  }
  cache->size = size;
  cache->sets = 0;

  hwloc_debug("cache L%u t%u linesize %u ways %u size %uKB\n", cache->level, cache->nbthreads_sharing, cache->linesize, cache->ways, cache->size >> 10);
}

/* Fetch information from the processor itself thanks to cpuid and store it in
 * infos for summarize to analyze them globally */
static void look_proc(struct procinfo *infos, unsigned highest_cpuid, unsigned highest_ext_cpuid, enum cpuid_type cpuid_type)
{
  unsigned eax, ebx, ecx = 0, edx;
  unsigned cachenum;
  struct cacheinfo *cache;

  infos->present = 1;

  eax = 0x01;
  hwloc_cpuid(&eax, &ebx, &ecx, &edx);
  infos->apicid = ebx >> 24;
  if (edx & (1 << 28))
    infos->max_log_proc = 1 << hwloc_flsl(((ebx >> 16) & 0xff) - 1);
  else
    infos->max_log_proc = 1;
  hwloc_debug("APIC ID 0x%02x max_log_proc %u\n", infos->apicid, infos->max_log_proc);
  infos->socketid = infos->apicid / infos->max_log_proc;
  infos->logprocid = infos->apicid % infos->max_log_proc;
  infos->coreid = (unsigned) -1;
  infos->threadid = (unsigned) -1;
  hwloc_debug("phys %u thread %u\n", infos->socketid, infos->logprocid);

  /* Intel doesn't actually provide 0x80000008 information */
  if (cpuid_type != intel && highest_ext_cpuid >= 0x80000008) {
    unsigned coreidsize;
    eax = 0x80000008;
    hwloc_cpuid(&eax, &ebx, &ecx, &edx);
    coreidsize = (ecx >> 12) & 0xf;
    hwloc_debug("core ID size: %u\n", coreidsize);
    if (!coreidsize) {
      infos->max_nbcores = (ecx & 0xff) + 1;
    } else 
      infos->max_nbcores = 1 << coreidsize;
    hwloc_debug("Thus max # of cores: %u\n", infos->max_nbcores);
    /* Still no multithreaded AMD */
    infos->max_nbthreads = 1 ;
    hwloc_debug("and max # of threads: %u\n", infos->max_nbthreads);
    infos->threadid = infos->logprocid % infos->max_nbthreads;
    infos->coreid = infos->logprocid / infos->max_nbthreads;
    hwloc_debug("this is thread %u of core %u\n", infos->threadid, infos->coreid);
  }

  infos->numcaches = 0;
  infos->cache = NULL;

  /* Intel doesn't actually provide 0x80000005 information */
  if (cpuid_type != intel && highest_ext_cpuid >= 0x80000005) {
    eax = 0x80000005;
    hwloc_cpuid(&eax, &ebx, &ecx, &edx);
    fill_amd_cache(infos, 1, ecx);
  }

  /* Intel doesn't actually provide 0x80000006 information */
  if (cpuid_type != intel && highest_ext_cpuid >= 0x80000006) {
    eax = 0x80000006;
    hwloc_cpuid(&eax, &ebx, &ecx, &edx);
    fill_amd_cache(infos, 2, ecx);
    fill_amd_cache(infos, 3, edx);
  }

  /* AMD doesn't actually provide 0x04 information */
  if (cpuid_type != amd && highest_cpuid >= 0x04) {
    cachenum = 0;
    for (cachenum = 0; ; cachenum++) {
      unsigned type;
      eax = 0x04;
      ecx = cachenum;
      hwloc_cpuid(&eax, &ebx, &ecx, &edx);

      type = eax & 0x1f;

      hwloc_debug("cache %u type %u\n", cachenum, type);

      if (type == 0)
	break;
      if (type == 2)
	/* Instruction cache */
	continue;
      infos->numcaches++;
    }

    cache = infos->cache = malloc(infos->numcaches * sizeof(*infos->cache));

    for (cachenum = 0; ; cachenum++) {
      unsigned linesize, linepart, ways, sets;
      unsigned type;
      eax = 0x04;
      ecx = cachenum;
      hwloc_cpuid(&eax, &ebx, &ecx, &edx);

      type = eax & 0x1f;

      if (type == 0)
	break;
      if (type == 2)
	/* Instruction cache */
	continue;

      cache->type = type;

      cache->level = (eax >> 5) & 0x7;
      cache->nbthreads_sharing = ((eax >> 14) & 0xfff) + 1;
      infos->max_nbcores = ((eax >> 26) & 0x3f) + 1;

      cache->linesize = linesize = (ebx & 0xfff) + 1;
      cache->linepart = linepart = ((ebx >> 12) & 0x3ff) + 1;
      cache->ways = ways = ((ebx >> 22) & 0x3ff) + 1;
      cache->sets = sets = ecx + 1;
      cache->size = linesize * linepart * ways * sets;

      hwloc_debug("cache %u type %u L%u t%u c%u linesize %u linepart %u ways %u sets %u, size %uKB\n", cachenum, cache->type, cache->level, cache->nbthreads_sharing, infos->max_nbcores, linesize, linepart, ways, sets, cache->size >> 10);
      infos->max_nbthreads = infos->max_log_proc / infos->max_nbcores;
      hwloc_debug("thus %u threads\n", infos->max_nbthreads);
      infos->threadid = infos->logprocid % infos->max_nbthreads;
      infos->coreid = infos->logprocid / infos->max_nbthreads;
      hwloc_debug("this is thread %u of core %u\n", infos->threadid, infos->coreid);

      cache++;
    }
  }

  if (cpuid_type == intel && highest_cpuid >= 0x0b) {
    unsigned level, apic_nextshift, apic_number, apic_type, apic_id = 0, apic_shift = 0, id;
    for (level = 0; ; level++) {
      ecx = level;
      eax = 0x0b;
      hwloc_cpuid(&eax, &ebx, &ecx, &edx);
      if (!eax && !ebx)
        break;
    }
    if (level) {
      infos->levels = level;
      infos->otherids = malloc(level * sizeof(*infos->otherids));
      for (level = 0; ; level++) {
	ecx = level;
	eax = 0x0b;
	hwloc_cpuid(&eax, &ebx, &ecx, &edx);
	if (!eax && !ebx)
	  break;
	apic_nextshift = eax & 0x1f;
	apic_number = ebx & 0xffff;
	apic_type = (ecx & 0xff00) >> 8;
	apic_id = edx;
	id = (apic_id >> apic_shift) & ((1 << (apic_nextshift - apic_shift)) - 1);
	hwloc_debug("x2APIC %08x %d: nextshift %d num %2d type %d id %2d\n", apic_id, level, apic_nextshift, apic_number, apic_type, id);
	infos->apicid = apic_id;
	infos->otherids[level] = UINT_MAX;
	switch (apic_type) {
	case 1:
	  infos->threadid = id;
	  break;
	case 2:
	  infos->coreid = id;
	  break;
	default:
	  hwloc_debug("x2APIC %d: unknown type %d\n", level, apic_type);
	  infos->otherids[level] = apic_id >> apic_shift;
	  break;
	}
	apic_shift = apic_nextshift;
      }
      infos->socketid = apic_id >> apic_shift;
      hwloc_debug("x2APIC remainder: %d\n", infos->socketid);
    } else
      infos->otherids = NULL;
  } else
    infos->otherids = NULL;
}

/* Analyse information stored in infos, and build topology levels accordingly */
static void summarize(hwloc_topology_t topology, struct procinfo *infos, unsigned nbprocs)
{
  hwloc_bitmap_t complete_cpuset = hwloc_bitmap_alloc();
  unsigned i, j, l, level;
  int one = -1;

  for (i = 0; i < nbprocs; i++)
    if (infos[i].present) {
      hwloc_bitmap_set(complete_cpuset, i);
      one = i;
    }

  if (one == -1)
    return;

  /* Look for sockets */
  {
    hwloc_bitmap_t sockets_cpuset = hwloc_bitmap_dup(complete_cpuset);
    hwloc_bitmap_t socket_cpuset;
    hwloc_obj_t sock;

    while ((i = hwloc_bitmap_first(sockets_cpuset)) != (unsigned) -1) {
      unsigned socketid = infos[i].socketid;

      socket_cpuset = hwloc_bitmap_alloc();
      for (j = i; j < nbprocs; j++) {
        if (infos[j].socketid == socketid) {
          hwloc_bitmap_set(socket_cpuset, j);
          hwloc_bitmap_clr(sockets_cpuset, j);
        }
      }
      sock = hwloc_alloc_setup_object(HWLOC_OBJ_SOCKET, socketid);
      sock->cpuset = socket_cpuset;
      hwloc_debug_1arg_bitmap("os socket %u has cpuset %s\n",
          socketid, socket_cpuset);
      hwloc_insert_object_by_cpuset(topology, sock);
    }
    hwloc_bitmap_free(sockets_cpuset);
  }

  /* Look for unknown objects */
  if (infos[one].otherids) {
    for (level = infos[one].levels-1; level <= infos[one].levels-1; level--) {
      if (infos[one].otherids[level] != UINT_MAX) {
	hwloc_bitmap_t unknowns_cpuset = hwloc_bitmap_dup(complete_cpuset);
	hwloc_bitmap_t unknown_cpuset;
	hwloc_obj_t unknown;

	while ((i = hwloc_bitmap_first(unknowns_cpuset)) != (unsigned) -1) {
	  unsigned unknownid = infos[i].otherids[level];

	  unknown_cpuset = hwloc_bitmap_alloc();
	  for (j = i; j < nbprocs; j++) {
	    if (infos[j].otherids[level] == unknownid) {
	      hwloc_bitmap_set(unknown_cpuset, j);
	      hwloc_bitmap_clr(unknowns_cpuset, j);
	    }
	  }
	  unknown = hwloc_alloc_setup_object(HWLOC_OBJ_MISC, unknownid);
	  unknown->cpuset = unknown_cpuset;
	  unknown->os_level = level;
	  hwloc_debug_2args_bitmap("os unknown%d %u has cpuset %s\n",
	      level, unknownid, unknown_cpuset);
	  hwloc_insert_object_by_cpuset(topology, unknown);
	}
	hwloc_bitmap_free(unknowns_cpuset);
      }
    }
  }

  /* Look for cores */
  {
    hwloc_bitmap_t cores_cpuset = hwloc_bitmap_dup(complete_cpuset);
    hwloc_bitmap_t core_cpuset;
    hwloc_obj_t core;

    while ((i = hwloc_bitmap_first(cores_cpuset)) != (unsigned) -1) {
      unsigned socketid = infos[i].socketid;
      unsigned coreid = infos[i].coreid;

      if (coreid == (unsigned) -1) {
        hwloc_bitmap_clr(cores_cpuset, i);
	continue;
      }

      core_cpuset = hwloc_bitmap_alloc();
      for (j = i; j < nbprocs; j++) {
	if (infos[j].coreid == (unsigned) -1) {
	  hwloc_bitmap_clr(cores_cpuset, j);
	  continue;
	}

        if (infos[j].socketid == socketid && infos[j].coreid == coreid) {
          hwloc_bitmap_set(core_cpuset, j);
          hwloc_bitmap_clr(cores_cpuset, j);
        }
      }
      core = hwloc_alloc_setup_object(HWLOC_OBJ_CORE, coreid);
      core->cpuset = core_cpuset;
      hwloc_debug_1arg_bitmap("os core %u has cpuset %s\n",
          coreid, core_cpuset);
      hwloc_insert_object_by_cpuset(topology, core);
    }
    hwloc_bitmap_free(cores_cpuset);
  }

  /* Look for caches */
  /* First find max level */
  level = 0;
  for (i = 0; i < nbprocs; i++)
    for (j = 0; j < infos[i].numcaches; j++)
      if (infos[i].cache[j].level > level)
        level = infos[i].cache[j].level;

  while (level > 0) {
    /* Look for caches at level level */
    {
      hwloc_bitmap_t caches_cpuset = hwloc_bitmap_dup(complete_cpuset);
      hwloc_bitmap_t cache_cpuset;
      hwloc_obj_t cache;

      while ((i = hwloc_bitmap_first(caches_cpuset)) != (unsigned) -1) {
        unsigned socketid = infos[i].socketid;

        for (l = 0; l < infos[i].numcaches; l++) {
          if (infos[i].cache[l].level == level)
            break;
        }
        if (l == infos[i].numcaches) {
          /* no cache Llevel in i, odd */
          hwloc_bitmap_clr(caches_cpuset, i);
          continue;
        }

        {
          unsigned cacheid = infos[i].apicid / infos[i].cache[l].nbthreads_sharing;

          cache_cpuset = hwloc_bitmap_alloc();
          for (j = i; j < nbprocs; j++) {
            unsigned l2;
            for (l2 = 0; l2 < infos[j].numcaches; l2++) {
              if (infos[j].cache[l2].level == level)
                break;
            }
            if (l2 == infos[j].numcaches) {
              /* no cache Llevel in j, odd */
              hwloc_bitmap_clr(caches_cpuset, j);
              continue;
            }
            if (infos[j].socketid == socketid && infos[j].apicid / infos[j].cache[l2].nbthreads_sharing == cacheid) {
              hwloc_bitmap_set(cache_cpuset, j);
              hwloc_bitmap_clr(caches_cpuset, j);
            }
          }
          cache = hwloc_alloc_setup_object(HWLOC_OBJ_CACHE, cacheid);
          cache->attr->cache.depth = level;
          cache->attr->cache.size = infos[i].cache[l].size;
          cache->attr->cache.linesize = infos[i].cache[l].linesize;
          cache->cpuset = cache_cpuset;
          hwloc_debug_2args_bitmap("os L%u cache %u has cpuset %s\n",
              level, cacheid, cache_cpuset);
          hwloc_insert_object_by_cpuset(topology, cache);
        }
      }
      hwloc_bitmap_free(caches_cpuset);
    }
    level--;
  }

  for (i = 0; i < nbprocs; i++) {
    free(infos[i].cache);
    if (infos[i].otherids)
      free(infos[i].otherids);
  }
}

#define INTEL_EBX ('G' | ('e'<<8) | ('n'<<16) | ('u'<<24))
#define INTEL_EDX ('i' | ('n'<<8) | ('e'<<16) | ('I'<<24))
#define INTEL_ECX ('n' | ('t'<<8) | ('e'<<16) | ('l'<<24))

#define AMD_EBX ('A' | ('u'<<8) | ('t'<<16) | ('h'<<24))
#define AMD_EDX ('e' | ('n'<<8) | ('t'<<16) | ('i'<<24))
#define AMD_ECX ('c' | ('A'<<8) | ('M'<<16) | ('D'<<24))

void hwloc_look_x86(struct hwloc_topology *topology, unsigned nbprocs)
{
    /* This function must always be here, but it's ok if it's empty. */
#if defined(HWLOC_HAVE_CPUID)
  unsigned eax, ebx, ecx = 0, edx;
  hwloc_bitmap_t orig_cpuset;
  unsigned i;
  unsigned highest_cpuid;
  unsigned highest_ext_cpuid;
  struct procinfo *infos = NULL;
  enum cpuid_type cpuid_type = unknown;

  if (!hwloc_have_cpuid())
    return;

  infos = malloc(sizeof(struct procinfo) * nbprocs);
  if (NULL == infos) {
      return;
  }

  eax = 0x00;
  hwloc_cpuid(&eax, &ebx, &ecx, &edx);
  highest_cpuid = eax;
  if (ebx == INTEL_EBX && ecx == INTEL_ECX && edx == INTEL_EDX)
    cpuid_type = intel;
  if (ebx == AMD_EBX && ecx == AMD_ECX && edx == AMD_EDX)
    cpuid_type = amd;

  hwloc_debug("highest cpuid %x, cpuid type %u\n", highest_cpuid, cpuid_type);
  if (highest_cpuid < 0x01) {
      goto free;
  }

  eax = 0x80000000;
  hwloc_cpuid(&eax, &ebx, &ecx, &edx);
  highest_ext_cpuid = eax;

  hwloc_debug("highest extended cpuid %x\n", highest_ext_cpuid);

  orig_cpuset = hwloc_bitmap_alloc();

  if (topology->get_thisthread_cpubind && topology->set_thisthread_cpubind) {
    if (!topology->get_thisthread_cpubind(topology, orig_cpuset, HWLOC_CPUBIND_STRICT)) {
      hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
      for (i = 0; i < nbprocs; i++) {
        hwloc_bitmap_only(cpuset, i);
        if (topology->set_thisthread_cpubind(topology, cpuset, HWLOC_CPUBIND_STRICT))
          continue;
        look_proc(&infos[i], highest_cpuid, highest_ext_cpuid, cpuid_type);
      }
      hwloc_bitmap_free(cpuset);
      topology->set_thisthread_cpubind(topology, orig_cpuset, 0);
      hwloc_bitmap_free(orig_cpuset);
      summarize(topology, infos, nbprocs);
      goto free;
    }
  }
  if (topology->get_thisproc_cpubind && topology->set_thisproc_cpubind) {
    if (!topology->get_thisproc_cpubind(topology, orig_cpuset, HWLOC_CPUBIND_STRICT)) {
      hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
      for (i = 0; i < nbprocs; i++) {
        hwloc_bitmap_only(cpuset, i);
        if (topology->set_thisproc_cpubind(topology, cpuset, HWLOC_CPUBIND_STRICT))
          continue;
        look_proc(&infos[i], highest_cpuid, highest_ext_cpuid, cpuid_type);
      }
      hwloc_bitmap_free(cpuset);
      topology->set_thisproc_cpubind(topology, orig_cpuset, 0);
      hwloc_bitmap_free(orig_cpuset);
      summarize(topology, infos, nbprocs);
      goto free;
    }
  }
#endif

  hwloc_add_object_info(topology->levels[0][0], "Backend", "x86");

 free:
  if (NULL != infos) {
      free(infos);
  }
}
