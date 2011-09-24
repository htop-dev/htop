/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2010 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include <private/autogen/config.h>

#define _ATFILE_SOURCE
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <float.h>

#include <hwloc.h>
#include <private/private.h>
#include <private/debug.h>

#ifdef HAVE_MACH_MACH_INIT_H
#include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_MACH_HOST_H
#include <mach/mach_host.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#ifdef HWLOC_WIN_SYS
#include <windows.h>
#endif

unsigned hwloc_get_api_version(void)
{
  return HWLOC_API_VERSION;
}

void hwloc_report_os_error(const char *msg, int line)
{
    static int reported = 0;

    if (!reported) {
        fprintf(stderr, "****************************************************************************\n");
        fprintf(stderr, "* Hwloc has encountered what looks like an error from the operating system.\n");
        fprintf(stderr, "*\n");
        fprintf(stderr, "* %s\n", msg);
        fprintf(stderr, "* Error occurred in topology.c line %d\n", line);
        fprintf(stderr, "*\n");
        fprintf(stderr, "* Please report this error message to the hwloc user's mailing list,\n");
        fprintf(stderr, "* along with the output from the hwloc-gather-topology.sh script.\n");
        fprintf(stderr, "****************************************************************************\n");
        reported = 1;
    }
}

static void
hwloc_topology_clear (struct hwloc_topology *topology);

#if defined(HAVE_SYSCTLBYNAME)
int hwloc_get_sysctlbyname(const char *name, int64_t *ret)
{
  union {
    int32_t i32;
    int64_t i64;
  } n;
  size_t size = sizeof(n);
  if (sysctlbyname(name, &n, &size, NULL, 0))
    return -1;
  switch (size) {
    case sizeof(n.i32):
      *ret = n.i32;
      break;
    case sizeof(n.i64):
      *ret = n.i64;
      break;
    default:
      return -1;
  }
  return 0;
}
#endif

#if defined(HAVE_SYSCTL)
int hwloc_get_sysctl(int name[], unsigned namelen, int *ret)
{
  int n;
  size_t size = sizeof(n);
  if (sysctl(name, namelen, &n, &size, NULL, 0))
    return -1;
  if (size != sizeof(n))
    return -1;
  *ret = n;
  return 0;
}
#endif

/* Return the OS-provided number of processors.  Unlike other methods such as
   reading sysfs on Linux, this method is not virtualizable; thus it's only
   used as a fall-back method, allowing `hwloc_set_fsroot ()' to
   have the desired effect.  */
unsigned
hwloc_fallback_nbprocessors(struct hwloc_topology *topology) {
  int n;
#if HAVE_DECL__SC_NPROCESSORS_ONLN
  n = sysconf(_SC_NPROCESSORS_ONLN);
#elif HAVE_DECL__SC_NPROC_ONLN
  n = sysconf(_SC_NPROC_ONLN);
#elif HAVE_DECL__SC_NPROCESSORS_CONF
  n = sysconf(_SC_NPROCESSORS_CONF);
#elif HAVE_DECL__SC_NPROC_CONF
  n = sysconf(_SC_NPROC_CONF);
#elif defined(HAVE_HOST_INFO) && HAVE_HOST_INFO
  struct host_basic_info info;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  host_info(mach_host_self(), HOST_BASIC_INFO, (integer_t*) &info, &count);
  n = info.avail_cpus;
#elif defined(HAVE_SYSCTLBYNAME)
  int64_t n;
  if (hwloc_get_sysctlbyname("hw.ncpu", &n))
    n = -1;
#elif defined(HAVE_SYSCTL) && HAVE_DECL_CTL_HW && HAVE_DECL_HW_NCPU
  static int name[2] = {CTL_HW, HW_NPCU};
  if (hwloc_get_sysctl(name, sizeof(name)/sizeof(*name)), &n)
    n = -1;
#elif defined(HWLOC_WIN_SYS)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  n = sysinfo.dwNumberOfProcessors;
#else
#ifdef __GNUC__
#warning No known way to discover number of available processors on this system
#warning hwloc_fallback_nbprocessors will default to 1
#endif
  n = -1;
#endif
  if (n >= 1)
    topology->support.discovery->pu = 1;
  else
    n = 1;
  return n;
}

/*
 * Use the given number of processors and the optional online cpuset if given
 * to set a PU level.
 */
void
hwloc_setup_pu_level(struct hwloc_topology *topology,
		     unsigned nb_pus)
{
  struct hwloc_obj *obj;
  unsigned oscpu,cpu;

  hwloc_debug("%s", "\n\n * CPU cpusets *\n\n");
  for (cpu=0,oscpu=0; cpu<nb_pus; oscpu++)
    {
      obj = hwloc_alloc_setup_object(HWLOC_OBJ_PU, oscpu);
      obj->cpuset = hwloc_bitmap_alloc();
      hwloc_bitmap_only(obj->cpuset, oscpu);

      hwloc_debug_2args_bitmap("cpu %u (os %u) has cpuset %s\n",
		 cpu, oscpu, obj->cpuset);
      hwloc_insert_object_by_cpuset(topology, obj);

      cpu++;
    }
}

static void
print_object(struct hwloc_topology *topology, int indent __hwloc_attribute_unused, hwloc_obj_t obj)
{
  char line[256], *cpuset = NULL;
  hwloc_debug("%*s", 2*indent, "");
  hwloc_obj_snprintf(line, sizeof(line), topology, obj, "#", 1);
  hwloc_debug("%s", line);
  if (obj->cpuset) {
    hwloc_bitmap_asprintf(&cpuset, obj->cpuset);
    hwloc_debug(" cpuset %s", cpuset);
    free(cpuset);
  }
  if (obj->complete_cpuset) {
    hwloc_bitmap_asprintf(&cpuset, obj->complete_cpuset);
    hwloc_debug(" complete %s", cpuset);
    free(cpuset);
  }
  if (obj->online_cpuset) {
    hwloc_bitmap_asprintf(&cpuset, obj->online_cpuset);
    hwloc_debug(" online %s", cpuset);
    free(cpuset);
  }
  if (obj->allowed_cpuset) {
    hwloc_bitmap_asprintf(&cpuset, obj->allowed_cpuset);
    hwloc_debug(" allowed %s", cpuset);
    free(cpuset);
  }
  if (obj->nodeset) {
    hwloc_bitmap_asprintf(&cpuset, obj->nodeset);
    hwloc_debug(" nodeset %s", cpuset);
    free(cpuset);
  }
  if (obj->complete_nodeset) {
    hwloc_bitmap_asprintf(&cpuset, obj->complete_nodeset);
    hwloc_debug(" completeN %s", cpuset);
    free(cpuset);
  }
  if (obj->allowed_nodeset) {
    hwloc_bitmap_asprintf(&cpuset, obj->allowed_nodeset);
    hwloc_debug(" allowedN %s", cpuset);
    free(cpuset);
  }
  if (obj->arity)
    hwloc_debug(" arity %u", obj->arity);
  hwloc_debug("%s", "\n");
}

/* Just for debugging.  */
static void
print_objects(struct hwloc_topology *topology __hwloc_attribute_unused, int indent __hwloc_attribute_unused, hwloc_obj_t obj __hwloc_attribute_unused)
{
#ifdef HWLOC_DEBUG
  print_object(topology, indent, obj);
  for (obj = obj->first_child; obj; obj = obj->next_sibling)
    print_objects(topology, indent + 1, obj);
#endif
}

void
hwloc_add_object_info(hwloc_obj_t obj, const char *name, const char *value)
{
#define OBJECT_INFO_ALLOC 8
  /* nothing allocated initially, (re-)allocate by multiple of 8 */
  unsigned alloccount = (obj->infos_count + 1 + (OBJECT_INFO_ALLOC-1)) & ~(OBJECT_INFO_ALLOC-1);
  if (obj->infos_count != alloccount)
    obj->infos = realloc(obj->infos, alloccount*sizeof(*obj->infos));
  obj->infos[obj->infos_count].name = strdup(name);
  obj->infos[obj->infos_count].value = strdup(value);
  obj->infos_count++;
}

static void
hwloc_clear_object_distances(hwloc_obj_t obj)
{
  unsigned i;
  for (i=0; i<obj->distances_count; i++)
    hwloc_free_logical_distances(obj->distances[i]);
  free(obj->distances);
  obj->distances = NULL;
  obj->distances_count = 0;
}

/* Free an object and all its content.  */
void
hwloc_free_unlinked_object(hwloc_obj_t obj)
{
  unsigned i;
  switch (obj->type) {
  default:
    break;
  }
  for(i=0; i<obj->infos_count; i++) {
    free(obj->infos[i].name);
    free(obj->infos[i].value);
  }
  free(obj->infos);
  hwloc_clear_object_distances(obj);
  free(obj->memory.page_types);
  free(obj->attr);
  free(obj->children);
  free(obj->name);
  hwloc_bitmap_free(obj->cpuset);
  hwloc_bitmap_free(obj->complete_cpuset);
  hwloc_bitmap_free(obj->online_cpuset);
  hwloc_bitmap_free(obj->allowed_cpuset);
  hwloc_bitmap_free(obj->nodeset);
  hwloc_bitmap_free(obj->complete_nodeset);
  hwloc_bitmap_free(obj->allowed_nodeset);
  free(obj);
}

/*
 * How to compare objects based on types.
 *
 * Note that HIGHER/LOWER is only a (consistent) heuristic, used to sort
 * objects with same cpuset consistently.
 * Only EQUAL / not EQUAL can be relied upon.
 */

enum hwloc_type_cmp_e {
  HWLOC_TYPE_HIGHER,
  HWLOC_TYPE_DEEPER,
  HWLOC_TYPE_EQUAL
};

/* WARNING: The indexes of this array MUST match the ordering that of
   the obj_order_type[] array, below.  Specifically, the values must
   be laid out such that:

       obj_order_type[obj_type_order[N]] = N

   for all HWLOC_OBJ_* values of N.  Put differently:

       obj_type_order[A] = B

   where the A values are in order of the hwloc_obj_type_t enum, and
   the B values are the corresponding indexes of obj_order_type.

   We can't use C99 syntax to initialize this in a little safer manner
   -- bummer.  :-( 

   *************************************************************
   *** DO NOT CHANGE THE ORDERING OF THIS ARRAY WITHOUT TRIPLE
   *** CHECKING ITS CORRECTNESS!
   *************************************************************
   */
static unsigned obj_type_order[] = {
    /* first entry is HWLOC_OBJ_SYSTEM */  0,
    /* next entry is HWLOC_OBJ_MACHINE */  1,
    /* next entry is HWLOC_OBJ_NODE */     3,
    /* next entry is HWLOC_OBJ_SOCKET */   4,
    /* next entry is HWLOC_OBJ_CACHE */    5,
    /* next entry is HWLOC_OBJ_CORE */     6,
    /* next entry is HWLOC_OBJ_PU */       7,
    /* next entry is HWLOC_OBJ_GROUP */    2,
    /* next entry is HWLOC_OBJ_MISC */     8,
};

static const hwloc_obj_type_t obj_order_type[] = {
  HWLOC_OBJ_SYSTEM,
  HWLOC_OBJ_MACHINE,
  HWLOC_OBJ_GROUP,
  HWLOC_OBJ_NODE,
  HWLOC_OBJ_SOCKET,
  HWLOC_OBJ_CACHE,
  HWLOC_OBJ_CORE,
  HWLOC_OBJ_PU,
  HWLOC_OBJ_MISC,
};

static unsigned __hwloc_attribute_const
hwloc_get_type_order(hwloc_obj_type_t type)
{
  return obj_type_order[type];
}

#if !defined(NDEBUG)
static hwloc_obj_type_t hwloc_get_order_type(int order)
{
  return obj_order_type[order];
}
#endif

int hwloc_compare_types (hwloc_obj_type_t type1, hwloc_obj_type_t type2)
{
  unsigned order1 = hwloc_get_type_order(type1);
  unsigned order2 = hwloc_get_type_order(type2);
  return order1 - order2;
}

static enum hwloc_type_cmp_e
hwloc_type_cmp(hwloc_obj_t obj1, hwloc_obj_t obj2)
{
  if (hwloc_compare_types(obj1->type, obj2->type) > 0)
    return HWLOC_TYPE_DEEPER;
  if (hwloc_compare_types(obj1->type, obj2->type) < 0)
    return HWLOC_TYPE_HIGHER;

  /* Caches have the same types but can have different depths.  */
  if (obj1->type == HWLOC_OBJ_CACHE) {
    if (obj1->attr->cache.depth < obj2->attr->cache.depth)
      return HWLOC_TYPE_DEEPER;
    else if (obj1->attr->cache.depth > obj2->attr->cache.depth)
      return HWLOC_TYPE_HIGHER;
  }

  /* Group objects have the same types but can have different depths.  */
  if (obj1->type == HWLOC_OBJ_GROUP) {
    if (obj1->attr->group.depth < obj2->attr->group.depth)
      return HWLOC_TYPE_DEEPER;
    else if (obj1->attr->group.depth > obj2->attr->group.depth)
      return HWLOC_TYPE_HIGHER;
  }

  return HWLOC_TYPE_EQUAL;
}

/*
 * How to compare objects based on cpusets.
 */

enum hwloc_obj_cmp_e {
  HWLOC_OBJ_EQUAL,	/**< \brief Equal */
  HWLOC_OBJ_INCLUDED,	/**< \brief Strictly included into */
  HWLOC_OBJ_CONTAINS,	/**< \brief Strictly contains */
  HWLOC_OBJ_INTERSECTS,	/**< \brief Intersects, but no inclusion! */
  HWLOC_OBJ_DIFFERENT	/**< \brief No intersection */
};

static int
hwloc_obj_cmp(hwloc_obj_t obj1, hwloc_obj_t obj2)
{
  if (!obj1->cpuset || hwloc_bitmap_iszero(obj1->cpuset)
      || !obj2->cpuset || hwloc_bitmap_iszero(obj2->cpuset))
    return HWLOC_OBJ_DIFFERENT;

  if (hwloc_bitmap_isequal(obj1->cpuset, obj2->cpuset)) {

    /* Same cpuset, subsort by type to have a consistent ordering.  */

    switch (hwloc_type_cmp(obj1, obj2)) {
      case HWLOC_TYPE_DEEPER:
	return HWLOC_OBJ_INCLUDED;
      case HWLOC_TYPE_HIGHER:
	return HWLOC_OBJ_CONTAINS;

      case HWLOC_TYPE_EQUAL:
        if (obj1->type == HWLOC_OBJ_MISC) {
          /* Misc objects may vary by name */
          int res = strcmp(obj1->name, obj2->name);
          if (res < 0)
            return HWLOC_OBJ_INCLUDED;
          if (res > 0)
            return HWLOC_OBJ_CONTAINS;
          if (res == 0)
            return HWLOC_OBJ_EQUAL;
        }

	/* Same level cpuset and type!  Let's hope it's coherent.  */
	return HWLOC_OBJ_EQUAL;
    }

    /* For dumb compilers */
    abort();

  } else {

    /* Different cpusets, sort by inclusion.  */

    if (hwloc_bitmap_isincluded(obj1->cpuset, obj2->cpuset))
      return HWLOC_OBJ_INCLUDED;

    if (hwloc_bitmap_isincluded(obj2->cpuset, obj1->cpuset))
      return HWLOC_OBJ_CONTAINS;

    if (hwloc_bitmap_intersects(obj1->cpuset, obj2->cpuset))
      return HWLOC_OBJ_INTERSECTS;

    return HWLOC_OBJ_DIFFERENT;
  }
}

/*
 * How to insert objects into the topology.
 *
 * Note: during detection, only the first_child and next_sibling pointers are
 * kept up to date.  Others are computed only once topology detection is
 * complete.
 */

#define merge_index(new, old, field, type) \
  if ((old)->field == (type) -1) \
    (old)->field = (new)->field;
#define merge_sizes(new, old, field) \
  if (!(old)->field) \
    (old)->field = (new)->field;
#ifdef HWLOC_DEBUG
#define check_sizes(new, old, field) \
  if ((new)->field) \
    assert((old)->field == (new)->field)
#else
#define check_sizes(new, old, field)
#endif

/* Try to insert OBJ in CUR, recurse if needed */
static int
hwloc___insert_object_by_cpuset(struct hwloc_topology *topology, hwloc_obj_t cur, hwloc_obj_t obj,
			        hwloc_report_error_t report_error)
{
  hwloc_obj_t child, container, *cur_children, *obj_children, next_child = NULL;
  int put;

  /* Make sure we haven't gone too deep.  */
  if (!hwloc_bitmap_isincluded(obj->cpuset, cur->cpuset)) {
    fprintf(stderr,"recursion has gone too deep?!\n");
    return -1;
  }

  /* Check whether OBJ is included in some child.  */
  container = NULL;
  for (child = cur->first_child; child; child = child->next_sibling) {
    switch (hwloc_obj_cmp(obj, child)) {
      case HWLOC_OBJ_EQUAL:
        merge_index(obj, child, os_level, signed);
	if (obj->os_level != child->os_level) {
          fprintf(stderr, "Different OS level\n");
          return -1;
        }
        merge_index(obj, child, os_index, unsigned);
	if (obj->os_index != child->os_index) {
          fprintf(stderr, "Different OS indexes\n");
          return -1;
        }
	switch(obj->type) {
	  case HWLOC_OBJ_NODE:
	    /* Do not check these, it may change between calls */
	    merge_sizes(obj, child, memory.local_memory);
	    merge_sizes(obj, child, memory.total_memory);
	    /* if both objects have a page_types array, just keep the biggest one for now */
	    if (obj->memory.page_types_len && child->memory.page_types_len)
	      hwloc_debug("%s", "merging page_types by keeping the biggest one only\n");
	    if (obj->memory.page_types_len < child->memory.page_types_len) {
	      free(obj->memory.page_types);
	    } else {
	      free(child->memory.page_types);
	      child->memory.page_types_len = obj->memory.page_types_len;
	      child->memory.page_types = obj->memory.page_types;
	      obj->memory.page_types = NULL;
	      obj->memory.page_types_len = 0;
	    }
	    break;
	  case HWLOC_OBJ_CACHE:
	    merge_sizes(obj, child, attr->cache.size);
	    check_sizes(obj, child, attr->cache.size);
	    merge_sizes(obj, child, attr->cache.linesize);
	    check_sizes(obj, child, attr->cache.linesize);
	    break;
	  default:
	    break;
	}
	/* Already present, no need to insert.  */
	return -1;
      case HWLOC_OBJ_INCLUDED:
	if (container) {
          if (report_error)
            report_error("object included in several different objects!", __LINE__);
	  /* We can't handle that.  */
	  return -1;
	}
	/* This child contains OBJ.  */
	container = child;
	break;
      case HWLOC_OBJ_INTERSECTS:
        if (report_error)
          report_error("object intersection without inclusion!", __LINE__);
	/* We can't handle that.  */
	return -1;
      case HWLOC_OBJ_CONTAINS:
	/* OBJ will be above CHILD.  */
	break;
      case HWLOC_OBJ_DIFFERENT:
	/* OBJ will be alongside CHILD.  */
	break;
    }
  }

  if (container) {
    /* OBJ is strictly contained is some child of CUR, go deeper.  */
    return hwloc___insert_object_by_cpuset(topology, container, obj, report_error);
  }

  /*
   * Children of CUR are either completely different from or contained into
   * OBJ. Take those that are contained (keeping sorting order), and sort OBJ
   * along those that are different.
   */

  /* OBJ is not put yet.  */
  put = 0;

  /* These will always point to the pointer to their next last child. */
  cur_children = &cur->first_child;
  obj_children = &obj->first_child;

  /* Construct CUR's and OBJ's children list.  */

  /* Iteration with prefetching to be completely safe against CHILD removal.  */
  for (child = cur->first_child, child ? next_child = child->next_sibling : NULL;
       child;
       child = next_child, child ? next_child = child->next_sibling : NULL) {

    switch (hwloc_obj_cmp(obj, child)) {

      case HWLOC_OBJ_DIFFERENT:
	/* Leave CHILD in CUR.  */
	if (!put && hwloc_bitmap_compare_first(obj->cpuset, child->cpuset) < 0) {
	  /* Sort children by cpuset: put OBJ before CHILD in CUR's children.  */
	  *cur_children = obj;
	  cur_children = &obj->next_sibling;
	  put = 1;
	}
	/* Now put CHILD in CUR's children.  */
	*cur_children = child;
	cur_children = &child->next_sibling;
	break;

      case HWLOC_OBJ_CONTAINS:
	/* OBJ contains CHILD, put the latter in the former.  */
	*obj_children = child;
	obj_children = &child->next_sibling;
	break;

      case HWLOC_OBJ_EQUAL:
      case HWLOC_OBJ_INCLUDED:
      case HWLOC_OBJ_INTERSECTS:
	/* Shouldn't ever happen as we have handled them above.  */
	abort();
    }
  }

  /* Put OBJ last in CUR's children if not already done so.  */
  if (!put) {
    *cur_children = obj;
    cur_children = &obj->next_sibling;
  }

  /* Close children lists.  */
  *obj_children = NULL;
  *cur_children = NULL;

  return 0;
}

/* insertion routine that lets you change the error reporting callback */
int
hwloc__insert_object_by_cpuset(struct hwloc_topology *topology, hwloc_obj_t obj,
			       hwloc_report_error_t report_error)
{
  int ret;
  /* Start at the top.  */
  /* Add the cpuset to the top */
  hwloc_bitmap_or(topology->levels[0][0]->complete_cpuset, topology->levels[0][0]->complete_cpuset, obj->cpuset);
  if (obj->nodeset)
    hwloc_bitmap_or(topology->levels[0][0]->complete_nodeset, topology->levels[0][0]->complete_nodeset, obj->nodeset);
  ret = hwloc___insert_object_by_cpuset(topology, topology->levels[0][0], obj, report_error);
  if (ret < 0)
    hwloc_free_unlinked_object(obj);
  return ret;
}

/* the default insertion routine warns in case of error.
 * it's used by most backends */
void
hwloc_insert_object_by_cpuset(struct hwloc_topology *topology, hwloc_obj_t obj)
{
  hwloc__insert_object_by_cpuset(topology, obj, hwloc_report_os_error);
}

void
hwloc_insert_object_by_parent(struct hwloc_topology *topology, hwloc_obj_t parent, hwloc_obj_t obj)
{
  hwloc_obj_t child, next_child = obj->first_child;
  hwloc_obj_t *current;

  /* Append to the end of the list */
  for (current = &parent->first_child; *current; current = &(*current)->next_sibling)
    ;
  *current = obj;
  obj->next_sibling = NULL;
  obj->first_child = NULL;

  /* Use the new object to insert children */
  parent = obj;

  /* Recursively insert children below */
  while (next_child) {
    child = next_child;
    next_child = child->next_sibling;
    hwloc_insert_object_by_parent(topology, parent, child);
  }
}

static void
hwloc_connect_children(hwloc_obj_t parent);
/* Adds a misc object _after_ detection, and thus has to reconnect all the pointers */
hwloc_obj_t
hwloc_topology_insert_misc_object_by_cpuset(struct hwloc_topology *topology, hwloc_const_bitmap_t cpuset, const char *name)
{
  hwloc_obj_t obj, child;
  int err;

  if (hwloc_bitmap_iszero(cpuset))
    return NULL;
  if (!hwloc_bitmap_isincluded(cpuset, hwloc_topology_get_complete_cpuset(topology)))
    return NULL;

  obj = hwloc_alloc_setup_object(HWLOC_OBJ_MISC, -1);
  if (name)
    obj->name = strdup(name);

  obj->cpuset = hwloc_bitmap_dup(cpuset);
  /* initialize default cpusets, we'll adjust them later */
  obj->complete_cpuset = hwloc_bitmap_dup(cpuset);
  obj->allowed_cpuset = hwloc_bitmap_dup(cpuset);
  obj->online_cpuset = hwloc_bitmap_dup(cpuset);

  err = hwloc__insert_object_by_cpuset(topology, obj, NULL /* do not show errors on stdout */);
  if (err < 0)
    return NULL;

  hwloc_connect_children(topology->levels[0][0]);

  if ((child = obj->first_child) != NULL && child->cpuset) {
    /* keep the main cpuset untouched, but update other cpusets and nodesets from children */
    obj->nodeset = hwloc_bitmap_alloc();
    obj->complete_nodeset = hwloc_bitmap_alloc();
    obj->allowed_nodeset = hwloc_bitmap_alloc();
    while (child) {
      if (child->complete_cpuset)
	hwloc_bitmap_or(obj->complete_cpuset, obj->complete_cpuset, child->complete_cpuset);
      if (child->allowed_cpuset)
	hwloc_bitmap_or(obj->allowed_cpuset, obj->allowed_cpuset, child->allowed_cpuset);
      if (child->online_cpuset)
	hwloc_bitmap_or(obj->online_cpuset, obj->online_cpuset, child->online_cpuset);
      if (child->nodeset)
	hwloc_bitmap_or(obj->nodeset, obj->nodeset, child->nodeset);
      if (child->complete_nodeset)
	hwloc_bitmap_or(obj->complete_nodeset, obj->complete_nodeset, child->complete_nodeset);
      if (child->allowed_nodeset)
	hwloc_bitmap_or(obj->allowed_nodeset, obj->allowed_nodeset, child->allowed_nodeset);
      child = child->next_sibling;
    }
  } else {
    /* copy the parent nodesets */
    obj->nodeset = hwloc_bitmap_dup(obj->parent->nodeset);
    obj->complete_nodeset = hwloc_bitmap_dup(obj->parent->complete_nodeset);
    obj->allowed_nodeset = hwloc_bitmap_dup(obj->parent->allowed_nodeset);
  }

  return obj;
}

hwloc_obj_t
hwloc_topology_insert_misc_object_by_parent(struct hwloc_topology *topology, hwloc_obj_t parent, const char *name)
{
  hwloc_obj_t obj = hwloc_alloc_setup_object(HWLOC_OBJ_MISC, -1);
  if (name)
    obj->name = strdup(name);

  hwloc_insert_object_by_parent(topology, parent, obj);

  hwloc_connect_children(topology->levels[0][0]);
  /* no need to hwloc_connect_levels() since misc object are not in levels */

  return obj;
}

/* Traverse children of a parent in a safe way: reread the next pointer as
 * appropriate to prevent crash on child deletion:  */
#define for_each_child_safe(child, parent, pchild) \
  for (pchild = &(parent)->first_child, child = *pchild; \
       child; \
       /* Check whether the current child was not dropped.  */ \
       (*pchild == child ? pchild = &(child->next_sibling) : NULL), \
       /* Get pointer to next childect.  */ \
        child = *pchild)

static int hwloc_memory_page_type_compare(const void *_a, const void *_b)
{
  const struct hwloc_obj_memory_page_type_s *a = _a;
  const struct hwloc_obj_memory_page_type_s *b = _b;
  /* consider 0 as larger so that 0-size page_type go to the end */
  return b->size ? (int)(a->size - b->size) : -1;
}

/* Propagate memory counts */
static void
propagate_total_memory(hwloc_obj_t obj)
{
  hwloc_obj_t *temp, child;
  unsigned i;

  /* reset total before counting local and children memory */
  obj->memory.total_memory = 0;

  /* Propagate memory up */
  for_each_child_safe(child, obj, temp) {
    propagate_total_memory(child);
    obj->memory.total_memory += child->memory.total_memory;
  }
  obj->memory.total_memory += obj->memory.local_memory;

  /* By the way, sort the page_type array.
   * Cannot do it on insert since some backends (e.g. XML) add page_types after inserting the object.
   */
  qsort(obj->memory.page_types, obj->memory.page_types_len, sizeof(*obj->memory.page_types), hwloc_memory_page_type_compare);
  /* Ignore 0-size page_types, they are at the end */
  for(i=obj->memory.page_types_len; i>=1; i--)
    if (obj->memory.page_types[i-1].size)
      break;
  obj->memory.page_types_len = i;
}

/* Collect the cpuset of all the PU objects. */
static void
collect_proc_cpuset(hwloc_obj_t obj, hwloc_obj_t sys)
{
  hwloc_obj_t child, *temp;

  if (sys) {
    /* We are already given a pointer to a system object */
    if (obj->type == HWLOC_OBJ_PU)
      hwloc_bitmap_or(sys->cpuset, sys->cpuset, obj->cpuset);
  } else {
    if (obj->cpuset) {
      /* This object is the root of a machine */
      sys = obj;
      /* Assume no PU for now */
      hwloc_bitmap_zero(obj->cpuset);
    }
  }

  for_each_child_safe(child, obj, temp)
    collect_proc_cpuset(child, sys);
}

/* While traversing down and up, propagate the offline/disallowed cpus by
 * and'ing them to and from the first object that has a cpuset */
static void
propagate_unused_cpuset(hwloc_obj_t obj, hwloc_obj_t sys)
{
  hwloc_obj_t child, *temp;

  if (obj->cpuset) {
    if (sys) {
      /* We are already given a pointer to an system object, update it and update ourselves */
      hwloc_bitmap_t mask = hwloc_bitmap_alloc();

      /* Apply the topology cpuset */
      hwloc_bitmap_and(obj->cpuset, obj->cpuset, sys->cpuset);

      /* Update complete cpuset down */
      if (obj->complete_cpuset) {
	hwloc_bitmap_and(obj->complete_cpuset, obj->complete_cpuset, sys->complete_cpuset);
      } else {
	obj->complete_cpuset = hwloc_bitmap_dup(sys->complete_cpuset);
	hwloc_bitmap_and(obj->complete_cpuset, obj->complete_cpuset, obj->cpuset);
      }

      /* Update online cpusets */
      if (obj->online_cpuset) {
	/* Update ours */
	hwloc_bitmap_and(obj->online_cpuset, obj->online_cpuset, sys->online_cpuset);

	/* Update the given cpuset, but only what we know */
	hwloc_bitmap_copy(mask, obj->cpuset);
	hwloc_bitmap_not(mask, mask);
	hwloc_bitmap_or(mask, mask, obj->online_cpuset);
	hwloc_bitmap_and(sys->online_cpuset, sys->online_cpuset, mask);
      } else {
	/* Just take it as such */
	obj->online_cpuset = hwloc_bitmap_dup(sys->online_cpuset);
	hwloc_bitmap_and(obj->online_cpuset, obj->online_cpuset, obj->cpuset);
      }

      /* Update allowed cpusets */
      if (obj->allowed_cpuset) {
	/* Update ours */
	hwloc_bitmap_and(obj->allowed_cpuset, obj->allowed_cpuset, sys->allowed_cpuset);

	/* Update the given cpuset, but only what we know */
	hwloc_bitmap_copy(mask, obj->cpuset);
	hwloc_bitmap_not(mask, mask);
	hwloc_bitmap_or(mask, mask, obj->allowed_cpuset);
	hwloc_bitmap_and(sys->allowed_cpuset, sys->allowed_cpuset, mask);
      } else {
	/* Just take it as such */
	obj->allowed_cpuset = hwloc_bitmap_dup(sys->allowed_cpuset);
	hwloc_bitmap_and(obj->allowed_cpuset, obj->allowed_cpuset, obj->cpuset);
      }

      hwloc_bitmap_free(mask);
    } else {
      /* This object is the root of a machine */
      sys = obj;
      /* Apply complete cpuset to cpuset, online_cpuset and allowed_cpuset, it
       * will automatically be applied below */
      if (obj->complete_cpuset)
        hwloc_bitmap_and(obj->cpuset, obj->cpuset, obj->complete_cpuset);
      else
        obj->complete_cpuset = hwloc_bitmap_dup(obj->cpuset);
      if (obj->online_cpuset)
        hwloc_bitmap_and(obj->online_cpuset, obj->online_cpuset, obj->complete_cpuset);
      else
        obj->online_cpuset = hwloc_bitmap_dup(obj->complete_cpuset);
      if (obj->allowed_cpuset)
        hwloc_bitmap_and(obj->allowed_cpuset, obj->allowed_cpuset, obj->complete_cpuset);
      else
        obj->allowed_cpuset = hwloc_bitmap_dup(obj->complete_cpuset);
    }
  }

  for_each_child_safe(child, obj, temp)
    propagate_unused_cpuset(child, sys);
}

/* Force full nodeset for non-NUMA machines */
static void
add_default_object_sets(hwloc_obj_t obj, int parent_has_sets)
{
  hwloc_obj_t child, *temp;

  if (parent_has_sets || obj->cpuset) {
    /* if the parent has non-NULL sets, or if the object has non-NULL cpusets,
     * it must have non-NULL nodesets
     */
    assert(obj->cpuset);
    assert(obj->online_cpuset);
    assert(obj->complete_cpuset);
    assert(obj->allowed_cpuset);
    if (!obj->nodeset)
      obj->nodeset = hwloc_bitmap_alloc_full();
    if (!obj->complete_nodeset)
      obj->complete_nodeset = hwloc_bitmap_alloc_full();
    if (!obj->allowed_nodeset)
      obj->allowed_nodeset = hwloc_bitmap_alloc_full();
  } else {
    /* parent has no sets and object has NULL cpusets,
     * it must have NULL nodesets
     */
    assert(!obj->nodeset);
    assert(!obj->complete_nodeset);
    assert(!obj->allowed_nodeset);
  }

  for_each_child_safe(child, obj, temp)
    add_default_object_sets(child, obj->cpuset != NULL);
}

/* Propagate nodesets up and down */
static void
propagate_nodeset(hwloc_obj_t obj, hwloc_obj_t sys)
{
  hwloc_obj_t child, *temp;
  hwloc_bitmap_t parent_nodeset = NULL;
  int parent_weight = 0;

  if (!sys && obj->nodeset) {
    sys = obj;
    if (!obj->complete_nodeset)
      obj->complete_nodeset = hwloc_bitmap_dup(obj->nodeset);
    if (!obj->allowed_nodeset)
      obj->allowed_nodeset = hwloc_bitmap_dup(obj->complete_nodeset);
  }

  if (sys) {
    if (obj->nodeset) {
      /* Some existing nodeset coming from above, to possibly propagate down */
      parent_nodeset = obj->nodeset;
      parent_weight = hwloc_bitmap_weight(parent_nodeset);
    } else
      obj->nodeset = hwloc_bitmap_alloc();
  }

  for_each_child_safe(child, obj, temp) {
    /* Propagate singleton nodesets down */
    if (parent_weight == 1) {
      if (!child->nodeset)
        child->nodeset = hwloc_bitmap_dup(obj->nodeset);
      else if (!hwloc_bitmap_isequal(child->nodeset, parent_nodeset)) {
        hwloc_debug_bitmap("Oops, parent nodeset %s", parent_nodeset);
        hwloc_debug_bitmap(" is different from child nodeset %s, ignoring the child one\n", child->nodeset);
        hwloc_bitmap_copy(child->nodeset, parent_nodeset);
      }
    }

    /* Recurse */
    propagate_nodeset(child, sys);

    /* Propagate children nodesets up */
    if (sys && child->nodeset)
      hwloc_bitmap_or(obj->nodeset, obj->nodeset, child->nodeset);
  }
}

/* Propagate allowed and complete nodesets */
static void
propagate_nodesets(hwloc_obj_t obj)
{
  hwloc_bitmap_t mask = hwloc_bitmap_alloc();
  hwloc_obj_t child, *temp;

  for_each_child_safe(child, obj, temp) {
    if (obj->nodeset) {
      /* Update complete nodesets down */
      if (child->complete_nodeset) {
        hwloc_bitmap_and(child->complete_nodeset, child->complete_nodeset, obj->complete_nodeset);
      } else if (child->nodeset) {
        child->complete_nodeset = hwloc_bitmap_dup(obj->complete_nodeset);
        hwloc_bitmap_and(child->complete_nodeset, child->complete_nodeset, child->nodeset);
      } /* else the child doesn't have nodeset information, we can not provide a complete nodeset */

      /* Update allowed nodesets down */
      if (child->allowed_nodeset) {
        hwloc_bitmap_and(child->allowed_nodeset, child->allowed_nodeset, obj->allowed_nodeset);
      } else if (child->nodeset) {
        child->allowed_nodeset = hwloc_bitmap_dup(obj->allowed_nodeset);
        hwloc_bitmap_and(child->allowed_nodeset, child->allowed_nodeset, child->nodeset);
      }
    }

    propagate_nodesets(child);

    if (obj->nodeset) {
      /* Update allowed nodesets up */
      if (child->nodeset && child->allowed_nodeset) {
        hwloc_bitmap_copy(mask, child->nodeset);
        hwloc_bitmap_andnot(mask, mask, child->allowed_nodeset);
        hwloc_bitmap_andnot(obj->allowed_nodeset, obj->allowed_nodeset, mask);
      }
    }
  }
  hwloc_bitmap_free(mask);

  if (obj->nodeset) {
    /* Apply complete nodeset to nodeset and allowed_nodeset */
    if (obj->complete_nodeset)
      hwloc_bitmap_and(obj->nodeset, obj->nodeset, obj->complete_nodeset);
    else
      obj->complete_nodeset = hwloc_bitmap_dup(obj->nodeset);
    if (obj->allowed_nodeset)
      hwloc_bitmap_and(obj->allowed_nodeset, obj->allowed_nodeset, obj->complete_nodeset);
    else
      obj->allowed_nodeset = hwloc_bitmap_dup(obj->complete_nodeset);
  }
}

static void
apply_nodeset(hwloc_obj_t obj, hwloc_obj_t sys)
{
  unsigned i;
  hwloc_obj_t child, *temp;

  if (sys) {
    if (obj->type == HWLOC_OBJ_NODE && obj->os_index != (unsigned) -1 &&
        !hwloc_bitmap_isset(sys->allowed_nodeset, obj->os_index)) {
      hwloc_debug("Dropping memory from disallowed node %u\n", obj->os_index);
      obj->memory.local_memory = 0;
      obj->memory.total_memory = 0;
      for(i=0; i<obj->memory.page_types_len; i++)
	obj->memory.page_types[i].count = 0;
    }
  } else {
    if (obj->allowed_nodeset) {
      sys = obj;
    }
  }

  for_each_child_safe(child, obj, temp)
    apply_nodeset(child, sys);
}

static void
remove_unused_cpusets(hwloc_obj_t obj)
{
  hwloc_obj_t child, *temp;

  if (obj->cpuset) {
    hwloc_bitmap_and(obj->cpuset, obj->cpuset, obj->online_cpuset);
    hwloc_bitmap_and(obj->cpuset, obj->cpuset, obj->allowed_cpuset);
  }

  for_each_child_safe(child, obj, temp)
    remove_unused_cpusets(child);
}

/* Remove an object from its parent and free it.
 * Only updates next_sibling/first_child pointers,
 * so may only be used during early discovery.
 * Children are inserted where the object was.
 */
static void
unlink_and_free_single_object(hwloc_obj_t *pparent)
{
  hwloc_obj_t parent = *pparent;
  hwloc_obj_t child = parent->first_child;
  /* Replace object with its list of children */
  if (child) {
    *pparent = child;
    while (child->next_sibling)
      child = child->next_sibling;
    child->next_sibling = parent->next_sibling;
  } else
    *pparent = parent->next_sibling;
  /* Remove ignored object */
  hwloc_free_unlinked_object(parent);
}

/* Remove all ignored objects.  */
static void
remove_ignored(hwloc_topology_t topology, hwloc_obj_t *pparent)
{
  hwloc_obj_t parent = *pparent, child, *pchild;

  for_each_child_safe(child, parent, pchild)
    remove_ignored(topology, pchild);

  if (parent != topology->levels[0][0] &&
      topology->ignored_types[parent->type] == HWLOC_IGNORE_TYPE_ALWAYS) {
    hwloc_debug("%s", "\nDropping ignored object ");
    print_object(topology, 0, parent);
    unlink_and_free_single_object(pparent);
  }
}

/* Remove an object and its children from its parent and free them.
 * Only updates next_sibling/first_child pointers,
 * so may only be used during early discovery.
 */
static void
unlink_and_free_object_and_children(hwloc_obj_t *pobj)
{
  hwloc_obj_t obj = *pobj, child, *pchild;

  for_each_child_safe(child, obj, pchild)
    unlink_and_free_object_and_children(pchild);

  *pobj = obj->next_sibling;
  hwloc_free_unlinked_object(obj);
}

/* Remove all children whose cpuset is empty, except NUMA nodes
 * since we want to keep memory information.  */
static void
remove_empty(hwloc_topology_t topology, hwloc_obj_t *pobj)
{
  hwloc_obj_t obj = *pobj, child, *pchild;

  for_each_child_safe(child, obj, pchild)
    remove_empty(topology, pchild);

  if (obj->type != HWLOC_OBJ_NODE
      && obj->cpuset /* FIXME: needed for PCI devices? */
      && hwloc_bitmap_iszero(obj->cpuset)) {
    /* Remove empty children */
    hwloc_debug("%s", "\nRemoving empty object ");
    print_object(topology, 0, obj);
    unlink_and_free_object_and_children(pobj);
  }
}

/* adjust object cpusets according the given droppedcpuset,
 * drop object whose cpuset becomes empty,
 * and mark dropped nodes in droppednodeset
 */
static void
restrict_object(hwloc_topology_t topology, unsigned long flags, hwloc_obj_t *pobj, hwloc_const_cpuset_t droppedcpuset, hwloc_nodeset_t droppednodeset, int droppingparent)
{
  hwloc_obj_t obj = *pobj, child, *pchild;
  int dropping;
  int modified = obj->complete_cpuset && hwloc_bitmap_intersects(obj->complete_cpuset, droppedcpuset);

  hwloc_clear_object_distances(obj);

  if (obj->cpuset)
    hwloc_bitmap_andnot(obj->cpuset, obj->cpuset, droppedcpuset);
  if (obj->complete_cpuset)
    hwloc_bitmap_andnot(obj->complete_cpuset, obj->complete_cpuset, droppedcpuset);
  if (obj->online_cpuset)
    hwloc_bitmap_andnot(obj->online_cpuset, obj->online_cpuset, droppedcpuset);
  if (obj->allowed_cpuset)
    hwloc_bitmap_andnot(obj->allowed_cpuset, obj->allowed_cpuset, droppedcpuset);

  if (obj->type == HWLOC_OBJ_MISC) {
    dropping = droppingparent && !(flags & HWLOC_RESTRICT_FLAG_ADAPT_MISC);
  } else {
    dropping = droppingparent || (obj->cpuset && hwloc_bitmap_iszero(obj->cpuset));
  }

  if (modified)
    for_each_child_safe(child, obj, pchild)
      restrict_object(topology, flags, pchild, droppedcpuset, droppednodeset, dropping);

  if (dropping) {
    hwloc_debug("%s", "\nRemoving object during restrict");
    print_object(topology, 0, obj);
    if (obj->type == HWLOC_OBJ_NODE)
      hwloc_bitmap_set(droppednodeset, obj->os_index);
    /* remove the object from the tree (no need to remove from levels, they will be entirely rebuilt by the caller) */
    unlink_and_free_single_object(pobj);
    /* do not remove children. if they were to be removed, they would have been already */
  }
}

/* adjust object nodesets accordingly the given droppednodeset
 */
static void
restrict_object_nodeset(hwloc_topology_t topology, hwloc_obj_t *pobj, hwloc_nodeset_t droppednodeset)
{
  hwloc_obj_t obj = *pobj, child, *pchild;

  /* if this object isn't modified, don't bother looking at children */
  if (obj->complete_nodeset && !hwloc_bitmap_intersects(obj->complete_nodeset, droppednodeset))
    return;

  if (obj->nodeset)
    hwloc_bitmap_andnot(obj->nodeset, obj->nodeset, droppednodeset);
  if (obj->complete_nodeset)
    hwloc_bitmap_andnot(obj->complete_nodeset, obj->complete_nodeset, droppednodeset);
  if (obj->allowed_nodeset)
    hwloc_bitmap_andnot(obj->allowed_nodeset, obj->allowed_nodeset, droppednodeset);

  for_each_child_safe(child, obj, pchild)
    restrict_object_nodeset(topology, pchild, droppednodeset);
}
/*
 * Merge with the only child if either the parent or the child has a type to be
 * ignored while keeping structure
 */
static void
merge_useless_child(hwloc_topology_t topology, hwloc_obj_t *pparent)
{
  hwloc_obj_t parent = *pparent, child, *pchild;

  for_each_child_safe(child, parent, pchild)
    merge_useless_child(topology, pchild);

  child = parent->first_child;
  if (!child || child->next_sibling)
    /* There are no or several children, it's useful to keep them.  */
    return;

  /* TODO: have a preference order?  */
  if (topology->ignored_types[parent->type] == HWLOC_IGNORE_TYPE_KEEP_STRUCTURE) {
    /* Parent can be ignored in favor of the child.  */
    hwloc_debug("%s", "\nIgnoring parent ");
    print_object(topology, 0, parent);
    *pparent = child;
    child->next_sibling = parent->next_sibling;
    hwloc_free_unlinked_object(parent);
  } else if (topology->ignored_types[child->type] == HWLOC_IGNORE_TYPE_KEEP_STRUCTURE) {
    /* Child can be ignored in favor of the parent.  */
    hwloc_debug("%s", "\nIgnoring child ");
    print_object(topology, 0, child);
    parent->first_child = child->first_child;
    hwloc_free_unlinked_object(child);
  }
}

/*
 * Initialize handy pointers in the whole topology.
 * The topology only had first_child and next_sibling pointers.
 * When this funtions return, all parent/children pointers are initialized.
 * The remaining fields (levels, cousins, logical_index, depth, ...) will
 * be setup later in hwloc_connect_levels().
 */
static void
hwloc_connect_children(hwloc_obj_t parent)
{
  unsigned n;
  hwloc_obj_t child, prev_child = NULL;

  for (n = 0, child = parent->first_child;
       child;
       n++,   prev_child = child, child = child->next_sibling) {
    child->parent = parent;
    child->sibling_rank = n;
    child->prev_sibling = prev_child;
  }
  parent->last_child = prev_child;

  parent->arity = n;
  free(parent->children);
  if (!n) {
    parent->children = NULL;
    return;
  }

  parent->children = malloc(n * sizeof(*parent->children));
  for (n = 0, child = parent->first_child;
       child;
       n++,   child = child->next_sibling) {
    parent->children[n] = child;
    hwloc_connect_children(child);
  }
}

/*
 * Check whether there is an object below ROOT that has the same type as OBJ
 */
static int
find_same_type(hwloc_obj_t root, hwloc_obj_t obj)
{
  hwloc_obj_t child;

  if (hwloc_type_cmp(root, obj) == HWLOC_TYPE_EQUAL)
    return 1;

  for (child = root->first_child; child; child = child->next_sibling)
    if (find_same_type(child, obj))
      return 1;

  return 0;
}

static int
hwloc_levels_ignore_object(hwloc_obj_t obj)
{
  return obj->type != HWLOC_OBJ_MISC;
}

/* traverse the array of current object and compare them with top_obj.
 * if equal, take the object and put its children into the remaining objs.
 * if not equal, put the object into the remaining objs.
 */
static int
hwloc_level_take_objects(hwloc_obj_t top_obj,
			 hwloc_obj_t *current_objs, unsigned n_current_objs,
			 hwloc_obj_t *taken_objs, unsigned n_taken_objs __hwloc_attribute_unused,
			 hwloc_obj_t *remaining_objs, unsigned n_remaining_objs __hwloc_attribute_unused)
{
  unsigned taken_i = 0;
  unsigned new_i = 0;
  unsigned ignored = 0;
  unsigned i, j;

  for (i = 0; i < n_current_objs; i++)
    if (hwloc_type_cmp(top_obj, current_objs[i]) == HWLOC_TYPE_EQUAL) {
      /* Take it, add children.  */
      taken_objs[taken_i++] = current_objs[i];
      for (j = 0; j < current_objs[i]->arity; j++) {
	hwloc_obj_t obj = current_objs[i]->children[j];
	if (hwloc_levels_ignore_object(obj))
	  remaining_objs[new_i++] = obj;
	else
	  ignored++;
      }
    } else {
      /* Leave it.  */
      hwloc_obj_t obj = current_objs[i];
      if (hwloc_levels_ignore_object(obj))
	remaining_objs[new_i++] = obj;
      else
	ignored++;
    }

#ifdef HWLOC_DEBUG
  /* Make sure we didn't mess up.  */
  assert(taken_i == n_taken_objs);
  assert(new_i + ignored == n_current_objs - n_taken_objs + n_remaining_objs);
#endif

  return new_i;
}

/*
 * Do the remaining work that hwloc_connect_children() did not do earlier.
 */
static int
hwloc_connect_levels(hwloc_topology_t topology)
{
  unsigned l, i=0;
  hwloc_obj_t *objs, *taken_objs, *new_objs, top_obj;
  unsigned n_objs, n_taken_objs, n_new_objs;

  /* reset non-root levels (root was initialized during init and will not change here) */
  for(l=1; l<HWLOC_DEPTH_MAX; l++)
    free(topology->levels[l]);
  memset(topology->levels+1, 0, (HWLOC_DEPTH_MAX-1)*sizeof(*topology->levels));
  memset(topology->level_nbobjects+1, 0,  (HWLOC_DEPTH_MAX-1)*sizeof(*topology->level_nbobjects));
  topology->nb_levels = 1;
  /* don't touch next_group_depth, the Group objects are still here */

  /* initialize all depth to unknown */
  for (l=1; l < HWLOC_OBJ_TYPE_MAX; l++)
    topology->type_depth[l] = HWLOC_TYPE_DEPTH_UNKNOWN;
  topology->type_depth[topology->levels[0][0]->type] = 0;

  /* Start with children of the whole system.  */
  l = 0;
  n_objs = topology->levels[0][0]->arity;
  objs = malloc(n_objs * sizeof(objs[0]));
  if (!objs) {
    errno = ENOMEM;
    hwloc_topology_clear(topology);
    return -1;
  }

  {
    hwloc_obj_t dummy_taken_objs;
    /* copy all root children that must go into levels,
     * root will go into dummy_taken_objs but we don't need it anyway
     * because it stays alone in first level.
     */
    n_objs = hwloc_level_take_objects(topology->levels[0][0],
				      topology->levels[0], 1,
				      &dummy_taken_objs, 1,
				      objs, n_objs);
#ifdef HWLOC_DEBUG
    assert(dummy_taken_objs == topology->levels[0][0]);
#endif
  }

  /* Keep building levels while there are objects left in OBJS.  */
  while (n_objs) {

    /* First find which type of object is the topmost.
     * Don't use PU if there are other types since we want to keep PU at the bottom.
     */
    for (i = 0; i < n_objs; i++)
      if (objs[i]->type != HWLOC_OBJ_PU)
        break;
    top_obj = i == n_objs ? objs[0] : objs[i];
    for (i = 0; i < n_objs; i++) {
      if (hwloc_type_cmp(top_obj, objs[i]) != HWLOC_TYPE_EQUAL) {
	if (find_same_type(objs[i], top_obj)) {
	  /* OBJS[i] is strictly above an object of the same type as TOP_OBJ, so it
	   * is above TOP_OBJ.  */
	  top_obj = objs[i];
	}
      }
    }

    /* Now peek all objects of the same type, build a level with that and
     * replace them with their children.  */

    /* First count them.  */
    n_taken_objs = 0;
    n_new_objs = 0;
    for (i = 0; i < n_objs; i++)
      if (hwloc_type_cmp(top_obj, objs[i]) == HWLOC_TYPE_EQUAL) {
	n_taken_objs++;
	n_new_objs += objs[i]->arity;
      }

    /* New level.  */
    taken_objs = malloc((n_taken_objs + 1) * sizeof(taken_objs[0]));
    /* New list of pending objects.  */
    new_objs = malloc((n_objs - n_taken_objs + n_new_objs) * sizeof(new_objs[0]));

    n_new_objs = hwloc_level_take_objects(top_obj,
					  objs, n_objs,
					  taken_objs, n_taken_objs,
					  new_objs, n_new_objs);

    /* Ok, put numbers in the level.  */
    for (i = 0; i < n_taken_objs; i++) {
      taken_objs[i]->depth = topology->nb_levels;
      taken_objs[i]->logical_index = i;
      if (i) {
	taken_objs[i]->prev_cousin = taken_objs[i-1];
	taken_objs[i-1]->next_cousin = taken_objs[i];
      }
    }

    /* One more level!  */
    if (top_obj->type == HWLOC_OBJ_CACHE)
      hwloc_debug("--- Cache level depth %u", top_obj->attr->cache.depth);
    else
      hwloc_debug("--- %s level", hwloc_obj_type_string(top_obj->type));
    hwloc_debug(" has number %u\n\n", topology->nb_levels);

    if (topology->type_depth[top_obj->type] == HWLOC_TYPE_DEPTH_UNKNOWN)
      topology->type_depth[top_obj->type] = topology->nb_levels;
    else
      topology->type_depth[top_obj->type] = HWLOC_TYPE_DEPTH_MULTIPLE; /* mark as unknown */

    taken_objs[n_taken_objs] = NULL;

    topology->level_nbobjects[topology->nb_levels] = n_taken_objs;
    topology->levels[topology->nb_levels] = taken_objs;

    topology->nb_levels++;

    free(objs);
    objs = new_objs;
    n_objs = n_new_objs;
  }

  /* It's empty now.  */
  free(objs);

  return 0;
}

/*
 * Empty binding hooks always returning success
 */

static int dontset_return_complete_cpuset(hwloc_topology_t topology, hwloc_cpuset_t set)
{
  hwloc_const_cpuset_t cpuset = hwloc_topology_get_complete_cpuset(topology);
  if (cpuset) {
    hwloc_bitmap_copy(set, hwloc_topology_get_complete_cpuset(topology));
    return 0;
  } else
    return -1;
}

static int dontset_thisthread_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_thisthread_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_bitmap_t set, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_cpuset(topology, set);
}
static int dontset_thisproc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_thisproc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_bitmap_t set, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_cpuset(topology, set);
}
static int dontset_proc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t pid __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_proc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t pid __hwloc_attribute_unused, hwloc_bitmap_t cpuset, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_cpuset(topology, cpuset);
}
#ifdef hwloc_thread_t
static int dontset_thread_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_thread_t tid __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_thread_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_thread_t tid __hwloc_attribute_unused, hwloc_bitmap_t cpuset, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_cpuset(topology, cpuset);
}
#endif

static int dontset_return_complete_nodeset(hwloc_topology_t topology, hwloc_nodeset_t set, hwloc_membind_policy_t *policy)
{
  hwloc_const_nodeset_t nodeset = hwloc_topology_get_complete_nodeset(topology);
  if (nodeset) {
    hwloc_bitmap_copy(set, hwloc_topology_get_complete_nodeset(topology));
    *policy = HWLOC_MEMBIND_DEFAULT;
    return 0;
  } else
    return -1;
}

static int dontset_thisproc_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, hwloc_membind_policy_t policy __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_thisproc_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_bitmap_t set, hwloc_membind_policy_t * policy, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_nodeset(topology, set, policy);
}

static int dontset_thisthread_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, hwloc_membind_policy_t policy __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_thisthread_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_bitmap_t set, hwloc_membind_policy_t * policy, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_nodeset(topology, set, policy);
}

static int dontset_proc_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t pid __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, hwloc_membind_policy_t policy __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_proc_membind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t pid __hwloc_attribute_unused, hwloc_bitmap_t set, hwloc_membind_policy_t * policy, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_nodeset(topology, set, policy);
}

static int dontset_area_membind(hwloc_topology_t topology __hwloc_attribute_unused, const void *addr __hwloc_attribute_unused, size_t size __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, hwloc_membind_policy_t policy __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return 0;
}
static int dontget_area_membind(hwloc_topology_t topology __hwloc_attribute_unused, const void *addr __hwloc_attribute_unused, size_t size __hwloc_attribute_unused, hwloc_bitmap_t set, hwloc_membind_policy_t * policy, int flags __hwloc_attribute_unused)
{
  return dontset_return_complete_nodeset(topology, set, policy);
}

static void * dontalloc_membind(hwloc_topology_t topology __hwloc_attribute_unused, size_t size __hwloc_attribute_unused, hwloc_const_bitmap_t set __hwloc_attribute_unused, hwloc_membind_policy_t policy __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  return malloc(size);
}
static int dontfree_membind(hwloc_topology_t topology __hwloc_attribute_unused, void *addr __hwloc_attribute_unused, size_t size __hwloc_attribute_unused)
{
  free(addr);
  return 0;
}

static void alloc_cpusets(hwloc_obj_t obj)
{
  obj->cpuset = hwloc_bitmap_alloc_full();
  obj->complete_cpuset = hwloc_bitmap_alloc();
  obj->online_cpuset = hwloc_bitmap_alloc_full();
  obj->allowed_cpuset = hwloc_bitmap_alloc_full();
  obj->nodeset = hwloc_bitmap_alloc();
  obj->complete_nodeset = hwloc_bitmap_alloc();
  obj->allowed_nodeset = hwloc_bitmap_alloc_full();
}

/* Main discovery loop */
static int
hwloc_discover(struct hwloc_topology *topology)
{
  if (topology->backend_type == HWLOC_BACKEND_SYNTHETIC) {
    alloc_cpusets(topology->levels[0][0]);
    hwloc_look_synthetic(topology);
#ifdef HWLOC_HAVE_XML
  } else if (topology->backend_type == HWLOC_BACKEND_XML) {
    hwloc_look_xml(topology);
#endif
  } else {

  /* Raw detection, from coarser levels to finer levels for more efficiency.  */

  /* hwloc_look_* functions should use hwloc_obj_add to add objects initialized
   * through hwloc_alloc_setup_object. For node levels, nodeset, memory_Kb and
   * huge_page_free must be initialized. For cache levels, memory_kB and
   * attr->cache.depth must be initialized. For misc levels, attr->misc.depth
   * must be initialized.
   */

  /* There must be at least a PU object for each logical processor, at worse
   * produced by hwloc_setup_pu_level()
   */

  /* To be able to just use hwloc_insert_object_by_cpuset to insert the object
   * in the topology according to the cpuset, the cpuset field must be
   * initialized.
   */

  /* A priori, All processors are visible in the topology, online, and allowed
   * for the application.
   *
   * - If some processors exist but topology information is unknown for them
   *   (and thus the backend couldn't create objects for them), they should be
   *   added to the complete_cpuset field of the lowest object where the object
   *   could reside.
   *
   * - If some processors are not online, they should be dropped from the
   *   online_cpuset field.
   *
   * - If some processors are not allowed for the application (e.g. for
   *   administration reasons), they should be dropped from the allowed_cpuset
   *   field.
   *
   * The same applies to the node sets complete_nodeset and allowed_cpuset.
   *
   * If such field doesn't exist yet, it can be allocated, and initialized to
   * zero (for complete), or to full (for online and allowed). The values are
   * automatically propagated to the whole tree after detection.
   *
   * Here, we only allocate cpusets for the root object.
   */

    alloc_cpusets(topology->levels[0][0]);

  /* Each OS type should also fill the bind functions pointers, at least the
   * set_cpubind one
   */

#    ifdef HWLOC_LINUX_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_linux(topology);
#    endif /* HWLOC_LINUX_SYS */

#    ifdef HWLOC_AIX_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_aix(topology);
#    endif /* HWLOC_AIX_SYS */

#    ifdef HWLOC_OSF_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_osf(topology);
#    endif /* HWLOC_OSF_SYS */

#    ifdef HWLOC_SOLARIS_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_solaris(topology);
#    endif /* HWLOC_SOLARIS_SYS */

#    ifdef HWLOC_WIN_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_windows(topology);
#    endif /* HWLOC_WIN_SYS */

#    ifdef HWLOC_DARWIN_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_darwin(topology);
#    endif /* HWLOC_DARWIN_SYS */

#    ifdef HWLOC_FREEBSD_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_freebsd(topology);
#    endif /* HWLOC_FREEBSD_SYS */

#    ifdef HWLOC_HPUX_SYS
#      define HAVE_OS_SUPPORT
    hwloc_look_hpux(topology);
#    endif /* HWLOC_HPUX_SYS */

#    ifndef HAVE_OS_SUPPORT
    hwloc_setup_pu_level(topology, hwloc_fallback_nbprocessors(topology));
#    endif /* Unsupported OS */


#    ifndef HWLOC_LINUX_SYS
    if (topology->is_thissystem) {
      /* gather uname info, except for Linux, which does it internally depending on load options */
      hwloc_add_uname_info(topology);
    }
#    endif
  }

  /*
   * Now that backends have detected objects, sort them and establish pointers.
   */
  print_objects(topology, 0, topology->levels[0][0]);

  /*
   * Group levels by distances
   */
  hwloc_convert_distances_indexes_into_objects(topology);
  hwloc_group_by_distances(topology);

  /* First tweak a bit to clean the topology.  */
  hwloc_debug("%s", "\nRestrict topology cpusets to existing PU and NODE objects\n");
  collect_proc_cpuset(topology->levels[0][0], NULL);

  hwloc_debug("%s", "\nPropagate offline and disallowed cpus down and up\n");
  propagate_unused_cpuset(topology->levels[0][0], NULL);

  if (topology->levels[0][0]->complete_nodeset && hwloc_bitmap_iszero(topology->levels[0][0]->complete_nodeset)) {
    /* No nodeset, drop all of them */
    hwloc_bitmap_free(topology->levels[0][0]->nodeset);
    topology->levels[0][0]->nodeset = NULL;
    hwloc_bitmap_free(topology->levels[0][0]->complete_nodeset);
    topology->levels[0][0]->complete_nodeset = NULL;
    hwloc_bitmap_free(topology->levels[0][0]->allowed_nodeset);
    topology->levels[0][0]->allowed_nodeset = NULL;
  }
  hwloc_debug("%s", "\nPropagate nodesets\n");
  propagate_nodeset(topology->levels[0][0], NULL);
  propagate_nodesets(topology->levels[0][0]);

  print_objects(topology, 0, topology->levels[0][0]);

  if (!(topology->flags & HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM)) {
    hwloc_debug("%s", "\nRemoving unauthorized and offline cpusets from all cpusets\n");
    remove_unused_cpusets(topology->levels[0][0]);

    hwloc_debug("%s", "\nRemoving disallowed memory according to nodesets\n");
    apply_nodeset(topology->levels[0][0], NULL);

    print_objects(topology, 0, topology->levels[0][0]);
  }

  hwloc_debug("%s", "\nRemoving ignored objects\n");
  remove_ignored(topology, &topology->levels[0][0]);

  print_objects(topology, 0, topology->levels[0][0]);

  hwloc_debug("%s", "\nRemoving empty objects except numa nodes and PCI devices\n");
  remove_empty(topology, &topology->levels[0][0]);

  if (!topology->levels[0][0]) {
    fprintf(stderr, "Topology became empty, aborting!\n");
    abort();
  }

  print_objects(topology, 0, topology->levels[0][0]);

  hwloc_debug("%s", "\nRemoving objects whose type has HWLOC_IGNORE_TYPE_KEEP_STRUCTURE and have only one child or are the only child\n");
  merge_useless_child(topology, &topology->levels[0][0]);

  print_objects(topology, 0, topology->levels[0][0]);

  hwloc_debug("%s", "\nAdd default object sets\n");
  add_default_object_sets(topology->levels[0][0], 0);

  hwloc_debug("%s", "\nOk, finished tweaking, now connect\n");

  /* Now connect handy pointers.  */

  hwloc_connect_children(topology->levels[0][0]);

  print_objects(topology, 0, topology->levels[0][0]);

  /* Explore the resulting topology level by level.  */

  if (hwloc_connect_levels(topology) < 0)
    return -1;

  /* accumulate children memory in total_memory fields (only once parent is set) */
  hwloc_debug("%s", "\nPropagate total memory up\n");
  propagate_total_memory(topology->levels[0][0]);

  /*
   * Now that objects are numbered, take distance matrices from backends and put them in the main topology
   */
  hwloc_finalize_logical_distances(topology);

#  ifdef HWLOC_HAVE_XML
  if (topology->backend_type == HWLOC_BACKEND_XML)
    /* make sure the XML-imported distances are ok now that the tree is properly setup */
    hwloc_xml_check_distances(topology);
#  endif

  /*
   * Now set binding hooks.
   * If the represented system is actually not this system, use dummy binding
   * hooks.
   */

  if (topology->flags & HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM)
    topology->is_thissystem = 1;

  if (topology->is_thissystem) {
#    ifdef HWLOC_LINUX_SYS
    hwloc_set_linux_hooks(topology);
#    endif /* HWLOC_LINUX_SYS */

#    ifdef HWLOC_AIX_SYS
    hwloc_set_aix_hooks(topology);
#    endif /* HWLOC_AIX_SYS */

#    ifdef HWLOC_OSF_SYS
    hwloc_set_osf_hooks(topology);
#    endif /* HWLOC_OSF_SYS */

#    ifdef HWLOC_SOLARIS_SYS
    hwloc_set_solaris_hooks(topology);
#    endif /* HWLOC_SOLARIS_SYS */

#    ifdef HWLOC_WIN_SYS
    hwloc_set_windows_hooks(topology);
#    endif /* HWLOC_WIN_SYS */

#    ifdef HWLOC_DARWIN_SYS
    hwloc_set_darwin_hooks(topology);
#    endif /* HWLOC_DARWIN_SYS */

#    ifdef HWLOC_FREEBSD_SYS
    hwloc_set_freebsd_hooks(topology);
#    endif /* HWLOC_FREEBSD_SYS */

#    ifdef HWLOC_HPUX_SYS
    hwloc_set_hpux_hooks(topology);
#    endif /* HWLOC_HPUX_SYS */
  } else {
    topology->set_thisproc_cpubind = dontset_thisproc_cpubind;
    topology->get_thisproc_cpubind = dontget_thisproc_cpubind;
    topology->set_thisthread_cpubind = dontset_thisthread_cpubind;
    topology->get_thisthread_cpubind = dontget_thisthread_cpubind;
    topology->set_proc_cpubind = dontset_proc_cpubind;
    topology->get_proc_cpubind = dontget_proc_cpubind;
#ifdef hwloc_thread_t
    topology->set_thread_cpubind = dontset_thread_cpubind;
    topology->get_thread_cpubind = dontget_thread_cpubind;
#endif
    topology->get_thisproc_last_cpu_location = dontget_thisproc_cpubind; /* cpubind instead of last_cpu_location is ok */
    topology->get_thisthread_last_cpu_location = dontget_thisthread_cpubind; /* cpubind instead of last_cpu_location is ok */
    topology->get_proc_last_cpu_location = dontget_proc_cpubind; /* cpubind instead of last_cpu_location is ok */
    /* TODO: get_thread_last_cpu_location */
    topology->set_thisproc_membind = dontset_thisproc_membind;
    topology->get_thisproc_membind = dontget_thisproc_membind;
    topology->set_thisthread_membind = dontset_thisthread_membind;
    topology->get_thisthread_membind = dontget_thisthread_membind;
    topology->set_proc_membind = dontset_proc_membind;
    topology->get_proc_membind = dontget_proc_membind;
    topology->set_area_membind = dontset_area_membind;
    topology->get_area_membind = dontget_area_membind;
    topology->alloc_membind = dontalloc_membind;
    topology->free_membind = dontfree_membind;
  }

  /* if not is_thissystem, set_cpubind is fake
   * and get_cpubind returns the whole system cpuset,
   * so don't report that set/get_cpubind as supported
   */
  if (topology->is_thissystem) {
#define DO(which,kind) \
    if (topology->kind) \
      topology->support.which##bind->kind = 1;
    DO(cpu,set_thisproc_cpubind);
    DO(cpu,get_thisproc_cpubind);
    DO(cpu,set_proc_cpubind);
    DO(cpu,get_proc_cpubind);
    DO(cpu,set_thisthread_cpubind);
    DO(cpu,get_thisthread_cpubind);
    DO(cpu,set_thread_cpubind);
    DO(cpu,get_thread_cpubind);
    DO(cpu,get_thisproc_last_cpu_location);
    DO(cpu,get_proc_last_cpu_location);
    DO(cpu,get_thisthread_last_cpu_location);
    DO(mem,set_thisproc_membind);
    DO(mem,get_thisproc_membind);
    DO(mem,set_thisthread_membind);
    DO(mem,get_thisthread_membind);
    DO(mem,set_proc_membind);
    DO(mem,get_proc_membind);
    DO(mem,set_area_membind);
    DO(mem,get_area_membind);
    DO(mem,alloc_membind);
  }

  return 0;
}

/* To be before discovery is actually launched,
 * Resets everything in case a previous load initialized some stuff.
 */
static void
hwloc_topology_setup_defaults(struct hwloc_topology *topology)
{
  struct hwloc_obj *root_obj;

  /* reset support */
  topology->set_thisproc_cpubind = NULL;
  topology->get_thisproc_cpubind = NULL;
  topology->set_thisthread_cpubind = NULL;
  topology->get_thisthread_cpubind = NULL;
  topology->set_proc_cpubind = NULL;
  topology->get_proc_cpubind = NULL;
#ifdef hwloc_thread_t
  topology->set_thread_cpubind = NULL;
  topology->get_thread_cpubind = NULL;
#endif
  topology->set_thisproc_membind = NULL;
  topology->get_thisproc_membind = NULL;
  topology->set_thisthread_membind = NULL;
  topology->get_thisthread_membind = NULL;
  topology->set_proc_membind = NULL;
  topology->get_proc_membind = NULL;
  topology->set_area_membind = NULL;
  topology->get_area_membind = NULL;
  topology->alloc = NULL;
  topology->alloc_membind = NULL;
  topology->free_membind = NULL;
  memset(topology->support.discovery, 0, sizeof(*topology->support.discovery));
  memset(topology->support.cpubind, 0, sizeof(*topology->support.cpubind));
  memset(topology->support.membind, 0, sizeof(*topology->support.membind));

  /* Only the System object on top by default */
  topology->nb_levels = 1; /* there's at least SYSTEM */
  topology->next_group_depth = 0;
  topology->levels[0] = malloc (sizeof (struct hwloc_obj));
  topology->level_nbobjects[0] = 1;
  /* NULLify other levels so that we can detect and free old ones in hwloc_connect_levels() if needed */
  memset(topology->levels+1, 0, (HWLOC_DEPTH_MAX-1)*sizeof(*topology->levels));

  /* Create the actual machine object, but don't touch its attributes yet
   * since the OS backend may still change the object into something else
   * (for instance System)
   */
  root_obj = hwloc_alloc_setup_object(HWLOC_OBJ_MACHINE, 0);
  root_obj->depth = 0;
  root_obj->logical_index = 0;
  root_obj->sibling_rank = 0;
  topology->levels[0][0] = root_obj;
}

int
hwloc_topology_init (struct hwloc_topology **topologyp)
{
  struct hwloc_topology *topology;
  int i;

  topology = malloc (sizeof (struct hwloc_topology));
  if(!topology)
    return -1;

  /* Setup topology context */
  topology->is_loaded = 0;
  topology->flags = 0;
  topology->is_thissystem = 1;
  topology->backend_type = HWLOC_BACKEND_NONE; /* backend not specified by default */
  topology->pid = 0;

  topology->support.discovery = malloc(sizeof(*topology->support.discovery));
  topology->support.cpubind = malloc(sizeof(*topology->support.cpubind));
  topology->support.membind = malloc(sizeof(*topology->support.membind));

  /* Only ignore useless cruft by default */
  for(i=0; i< HWLOC_OBJ_TYPE_MAX; i++)
    topology->ignored_types[i] = HWLOC_IGNORE_TYPE_NEVER;
  topology->ignored_types[HWLOC_OBJ_GROUP] = HWLOC_IGNORE_TYPE_KEEP_STRUCTURE;

  hwloc_topology_distances_init(topology);

  /* Make the topology look like something coherent but empty */
  hwloc_topology_setup_defaults(topology);

  *topologyp = topology;
  return 0;
}

static void
hwloc_backend_exit(struct hwloc_topology *topology)
{
  switch (topology->backend_type) {
#ifdef HWLOC_LINUX_SYS
  case HWLOC_BACKEND_SYSFS:
    hwloc_backend_sysfs_exit(topology);
    break;
#endif
#ifdef HWLOC_HAVE_XML
  case HWLOC_BACKEND_XML:
    hwloc_backend_xml_exit(topology);
    break;
#endif
  case HWLOC_BACKEND_SYNTHETIC:
    hwloc_backend_synthetic_exit(topology);
    break;
  default:
    break;
  }

  assert(topology->backend_type == HWLOC_BACKEND_NONE);
}

int
hwloc_topology_set_fsroot(struct hwloc_topology *topology, const char *fsroot_path __hwloc_attribute_unused)
{
  /* cleanup existing backend */
  hwloc_backend_exit(topology);

#ifdef HWLOC_LINUX_SYS
  if (hwloc_backend_sysfs_init(topology, fsroot_path) < 0)
    return -1;
#endif /* HWLOC_LINUX_SYS */

  return 0;
}

int
hwloc_topology_set_pid(struct hwloc_topology *topology __hwloc_attribute_unused,
                       hwloc_pid_t pid __hwloc_attribute_unused)
{
#ifdef HWLOC_LINUX_SYS
  topology->pid = pid;
  return 0;
#else /* HWLOC_LINUX_SYS */
  errno = ENOSYS;
  return -1;
#endif /* HWLOC_LINUX_SYS */
}

int
hwloc_topology_set_synthetic(struct hwloc_topology *topology, const char *description)
{
  /* cleanup existing backend */
  hwloc_backend_exit(topology);

  return hwloc_backend_synthetic_init(topology, description);
}

int
hwloc_topology_set_xml(struct hwloc_topology *topology __hwloc_attribute_unused,
                       const char *xmlpath __hwloc_attribute_unused)
{
#ifdef HWLOC_HAVE_XML
  /* cleanup existing backend */
  hwloc_backend_exit(topology);

  return hwloc_backend_xml_init(topology, xmlpath, NULL, 0);
#else /* HWLOC_HAVE_XML */
  errno = ENOSYS;
  return -1;
#endif /* !HWLOC_HAVE_XML */
}

int
hwloc_topology_set_xmlbuffer(struct hwloc_topology *topology __hwloc_attribute_unused,
                             const char *xmlbuffer __hwloc_attribute_unused,
                             int size __hwloc_attribute_unused)
{
#ifdef HWLOC_HAVE_XML
  /* cleanup existing backend */
  hwloc_backend_exit(topology);

  return hwloc_backend_xml_init(topology, NULL, xmlbuffer, size);
#else /* HWLOC_HAVE_XML */
  errno = ENOSYS;
  return -1;
#endif /* !HWLOC_HAVE_XML */
}

int
hwloc_topology_set_flags (struct hwloc_topology *topology, unsigned long flags)
{
  topology->flags = flags;
  return 0;
}

int
hwloc_topology_ignore_type(struct hwloc_topology *topology, hwloc_obj_type_t type)
{
  if (type >= HWLOC_OBJ_TYPE_MAX) {
    errno = EINVAL;
    return -1;
  }

  if (type == HWLOC_OBJ_PU) {
    /* we need the PU level */
    errno = EINVAL;
    return -1;
  }

  topology->ignored_types[type] = HWLOC_IGNORE_TYPE_ALWAYS;
  return 0;
}

int
hwloc_topology_ignore_type_keep_structure(struct hwloc_topology *topology, hwloc_obj_type_t type)
{
  if (type >= HWLOC_OBJ_TYPE_MAX) {
    errno = EINVAL;
    return -1;
  }

  if (type == HWLOC_OBJ_PU) {
    /* we need the PU level */
    errno = EINVAL;
    return -1;
  }

  topology->ignored_types[type] = HWLOC_IGNORE_TYPE_KEEP_STRUCTURE;
  return 0;
}

int
hwloc_topology_ignore_all_keep_structure(struct hwloc_topology *topology)
{
  unsigned type;
  for(type=0; type<HWLOC_OBJ_TYPE_MAX; type++)
    if (type != HWLOC_OBJ_PU)
      topology->ignored_types[type] = HWLOC_IGNORE_TYPE_KEEP_STRUCTURE;
  return 0;
}

static void
hwloc_topology_clear_tree (struct hwloc_topology *topology, struct hwloc_obj *root)
{
  unsigned i;
  for(i=0; i<root->arity; i++)
    hwloc_topology_clear_tree (topology, root->children[i]);
  hwloc_free_unlinked_object (root);
}

static void
hwloc_topology_clear (struct hwloc_topology *topology)
{
  unsigned l;
  hwloc_topology_distances_clear(topology);
  hwloc_topology_clear_tree (topology, topology->levels[0][0]);
  for (l=0; l<topology->nb_levels; l++)
    free(topology->levels[l]);
}

void
hwloc_topology_destroy (struct hwloc_topology *topology)
{
  hwloc_topology_clear(topology);
  hwloc_topology_distances_destroy(topology);
  hwloc_backend_exit(topology);
  free(topology->support.discovery);
  free(topology->support.cpubind);
  free(topology->support.membind);
  free(topology);
}

int
hwloc_topology_load (struct hwloc_topology *topology)
{
  char *local_env;
  int err;

  if (topology->is_loaded) {
    hwloc_topology_clear(topology);
    hwloc_topology_setup_defaults(topology);
    topology->is_loaded = 0;
  }

  /* enforce backend anyway if a FORCE variable was given */
#ifdef HWLOC_LINUX_SYS
  {
    char *fsroot_path_env = getenv("HWLOC_FORCE_FSROOT");
    if (fsroot_path_env) {
      hwloc_backend_exit(topology);
      hwloc_backend_sysfs_init(topology, fsroot_path_env);
    }
  }
#endif
#ifdef HWLOC_HAVE_XML
  {
    char *xmlpath_env = getenv("HWLOC_FORCE_XMLFILE");
    if (xmlpath_env) {
      hwloc_backend_exit(topology);
      hwloc_backend_xml_init(topology, xmlpath_env, NULL, 0);
    }
  }
#endif

  /* only apply non-FORCE variables if we have not changed the backend yet */
#ifdef HWLOC_LINUX_SYS
  if (topology->backend_type == HWLOC_BACKEND_NONE) {
    char *fsroot_path_env = getenv("HWLOC_FSROOT");
    if (fsroot_path_env)
      hwloc_backend_sysfs_init(topology, fsroot_path_env);
  }
#endif
#ifdef HWLOC_HAVE_XML
  if (topology->backend_type == HWLOC_BACKEND_NONE) {
    char *xmlpath_env = getenv("HWLOC_XMLFILE");
    if (xmlpath_env)
      hwloc_backend_xml_init(topology, xmlpath_env, NULL, 0);
  }
#endif

  /* always apply non-FORCE THISSYSTEM since it was explicitly designed to override setups from other backends */
  local_env = getenv("HWLOC_THISSYSTEM");
  if (local_env)
    topology->is_thissystem = atoi(local_env);

  /* if we haven't chosen the backend, set the OS-specific one if needed */
  if (topology->backend_type == HWLOC_BACKEND_NONE) {
#ifdef HWLOC_LINUX_SYS
    if (hwloc_backend_sysfs_init(topology, "/") < 0)
      return -1;
#endif
  }

  /* get distance matrix from the environment are store them (as indexes) in the topology.
   * indexes will be converted into objects later once the tree will be filled
   */
  hwloc_store_distances_from_env(topology);

  /* actual topology discovery */
  err = hwloc_discover(topology);
  if (err < 0)
    return err;

  /* enforce THISSYSTEM if given in a FORCE variable */
  local_env = getenv("HWLOC_FORCE_THISSYSTEM");
  if (local_env)
    topology->is_thissystem = atoi(local_env);

#ifndef HWLOC_DEBUG
  if (getenv("HWLOC_DEBUG_CHECK"))
#endif
    hwloc_topology_check(topology);

  topology->is_loaded = 1;
  return 0;
}

int
hwloc_topology_restrict(struct hwloc_topology *topology, hwloc_const_cpuset_t cpuset, unsigned long flags)
{
  hwloc_bitmap_t droppedcpuset, droppednodeset;

  /* make sure we'll keep something in the topology */
  if (!hwloc_bitmap_intersects(cpuset, topology->levels[0][0]->cpuset)) {
    errno = EINVAL;
    return -1;
  }

  droppedcpuset = hwloc_bitmap_alloc();
  droppednodeset = hwloc_bitmap_alloc();

  /* drop object based on the reverse of cpuset, and fill the 'dropped' nodeset */
  hwloc_bitmap_not(droppedcpuset, cpuset);
  restrict_object(topology, flags, &topology->levels[0][0], droppedcpuset, droppednodeset, 0 /* root cannot be removed */);
  /* update nodesets according to dropped nodeset */
  restrict_object_nodeset(topology, &topology->levels[0][0], droppednodeset);

  hwloc_bitmap_free(droppedcpuset);
  hwloc_bitmap_free(droppednodeset);

  hwloc_connect_children(topology->levels[0][0]);
  hwloc_connect_levels(topology);
  propagate_total_memory(topology->levels[0][0]);
  hwloc_restrict_distances(topology, flags);
  hwloc_convert_distances_indexes_into_objects(topology);
  hwloc_finalize_logical_distances(topology);
  return 0;
}

int
hwloc_topology_is_thissystem(struct hwloc_topology *topology)
{
  return topology->is_thissystem;
}

unsigned
hwloc_topology_get_depth(struct hwloc_topology *topology) 
{
  return topology->nb_levels;
}

/* check children between a parent object */
static void
hwloc__check_children(struct hwloc_obj *parent)
{
  hwloc_bitmap_t remaining_parent_set;
  unsigned j;

  if (!parent->arity) {
    /* check whether that parent has no children for real */
    assert(!parent->children);
    assert(!parent->first_child);
    assert(!parent->last_child);
    return;
  }
  /* check whether that parent has children for real */
  assert(parent->children);
  assert(parent->first_child);
  assert(parent->last_child);

  /* first child specific checks */
  assert(parent->first_child->sibling_rank == 0);
  assert(parent->first_child == parent->children[0]);
  assert(parent->first_child->prev_sibling == NULL);

  /* last child specific checks */
  assert(parent->last_child->sibling_rank == parent->arity-1);
  assert(parent->last_child == parent->children[parent->arity-1]);
  assert(parent->last_child->next_sibling == NULL);

  if (parent->cpuset) {
    remaining_parent_set = hwloc_bitmap_dup(parent->cpuset);
    for(j=0; j<parent->arity; j++) {
      if (!parent->children[j]->cpuset)
	continue;
      /* check that child cpuset is included in the parent */
      assert(hwloc_bitmap_isincluded(parent->children[j]->cpuset, remaining_parent_set));
#if !defined(NDEBUG)
      /* check that children are correctly ordered (see below), empty ones may be anywhere */
      if (!hwloc_bitmap_iszero(parent->children[j]->cpuset)) {
        int firstchild = hwloc_bitmap_first(parent->children[j]->cpuset);
        int firstparent = hwloc_bitmap_first(remaining_parent_set);
        assert(firstchild == firstparent);
      }
#endif
      /* clear previously used parent cpuset bits so that we actually checked above
       * that children cpusets do not intersect and are ordered properly
       */
      hwloc_bitmap_andnot(remaining_parent_set, remaining_parent_set, parent->children[j]->cpuset);
    }
    assert(hwloc_bitmap_iszero(remaining_parent_set));
    hwloc_bitmap_free(remaining_parent_set);
  }

  /* checks for all children */
  for(j=1; j<parent->arity; j++) {
    assert(parent->children[j]->sibling_rank == j);
    assert(parent->children[j-1]->next_sibling == parent->children[j]);
    assert(parent->children[j]->prev_sibling == parent->children[j-1]);
  }
}

/* check a whole topology structure */
void
hwloc_topology_check(struct hwloc_topology *topology)
{
  struct hwloc_obj *obj;
  hwloc_obj_type_t type;
  unsigned i, j, depth;

  /* check type orders */
  for (type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    assert(hwloc_get_order_type(hwloc_get_type_order(type)) == type);
  }
  for (i = hwloc_get_type_order(HWLOC_OBJ_SYSTEM);
       i <= hwloc_get_type_order(HWLOC_OBJ_CORE); i++) {
    assert(i == hwloc_get_type_order(hwloc_get_order_type(i)));
  }

  /* check that last level is PU */
  assert(hwloc_get_depth_type(topology, hwloc_topology_get_depth(topology)-1) == HWLOC_OBJ_PU);
  /* check that other levels are not PU */
  for(i=1; i<hwloc_topology_get_depth(topology)-1; i++)
    assert(hwloc_get_depth_type(topology, i) != HWLOC_OBJ_PU);

  /* top-level specific checks */
  assert(hwloc_get_nbobjs_by_depth(topology, 0) == 1);
  obj = hwloc_get_root_obj(topology);
  assert(obj);

  depth = hwloc_topology_get_depth(topology);

  /* check each level */
  for(i=0; i<depth; i++) {
    unsigned width = hwloc_get_nbobjs_by_depth(topology, i);
    struct hwloc_obj *prev = NULL;

    /* check each object of the level */
    for(j=0; j<width; j++) {
      obj = hwloc_get_obj_by_depth(topology, i, j);
      /* check that the object is corrected placed horizontally and vertically */
      assert(obj);
      assert(obj->depth == i);
      assert(obj->logical_index == j);
      /* check that all objects in the level have the same type */
      if (prev) {
	assert(hwloc_type_cmp(obj, prev) == HWLOC_TYPE_EQUAL);
	assert(prev->next_cousin == obj);
	assert(obj->prev_cousin == prev);
      }
      if (obj->complete_cpuset) {
        if (obj->cpuset)
          assert(hwloc_bitmap_isincluded(obj->cpuset, obj->complete_cpuset));
        if (obj->online_cpuset)
          assert(hwloc_bitmap_isincluded(obj->online_cpuset, obj->complete_cpuset));
        if (obj->allowed_cpuset)
          assert(hwloc_bitmap_isincluded(obj->allowed_cpuset, obj->complete_cpuset));
      }
      if (obj->complete_nodeset) {
        if (obj->nodeset)
          assert(hwloc_bitmap_isincluded(obj->nodeset, obj->complete_nodeset));
        if (obj->allowed_nodeset)
          assert(hwloc_bitmap_isincluded(obj->allowed_nodeset, obj->complete_nodeset));
      }
      /* check children */
      hwloc__check_children(obj);
      prev = obj;
    }

    /* check first object of the level */
    obj = hwloc_get_obj_by_depth(topology, i, 0);
    assert(obj);
    assert(!obj->prev_cousin);

    /* check type */
    assert(hwloc_get_depth_type(topology, i) == obj->type);
    assert(i == (unsigned) hwloc_get_type_depth(topology, obj->type) ||
           HWLOC_TYPE_DEPTH_MULTIPLE == hwloc_get_type_depth(topology, obj->type));

    /* check last object of the level */
    obj = hwloc_get_obj_by_depth(topology, i, width-1);
    assert(obj);
    assert(!obj->next_cousin);

    /* check last+1 object of the level */
    obj = hwloc_get_obj_by_depth(topology, i, width);
    assert(!obj);
  }

  /* check bottom objects */
  assert(hwloc_get_nbobjs_by_depth(topology, depth-1) > 0);
  for(j=0; j<hwloc_get_nbobjs_by_depth(topology, depth-1); j++) {
    obj = hwloc_get_obj_by_depth(topology, depth-1, j);
    assert(obj);
    /* bottom-level object must always be PU */
    assert(obj->type == HWLOC_OBJ_PU);
  }
}

const struct hwloc_topology_support *
hwloc_topology_get_support(struct hwloc_topology * topology)
{
  return &topology->support;
}
