/*
 * Copyright © 2009      CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 *
 * See COPYING in top-level directory.
 */

/* Internal types and helpers. */

#ifndef HWLOC_PRIVATE_H
#define HWLOC_PRIVATE_H

#include <private/autogen/config.h>
#include <hwloc.h>
#include <hwloc/bitmap.h>
#include <private/debug.h>
#include <sys/types.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#ifdef HWLOC_HAVE_ATTRIBUTE_FORMAT
# if HWLOC_HAVE_ATTRIBUTE_FORMAT
#  define __hwloc_attribute_format(type, str, arg)  __attribute__((__format__(type, str, arg)))
# else
#  define __hwloc_attribute_format(type, str, arg)
# endif
#else
# define __hwloc_attribute_format(type, str, arg)
#endif

enum hwloc_ignore_type_e {
  HWLOC_IGNORE_TYPE_NEVER = 0,
  HWLOC_IGNORE_TYPE_KEEP_STRUCTURE,
  HWLOC_IGNORE_TYPE_ALWAYS
};

#define HWLOC_DEPTH_MAX 128

typedef enum hwloc_backend_e {
  HWLOC_BACKEND_NONE,
  HWLOC_BACKEND_SYNTHETIC,
#ifdef HWLOC_LINUX_SYS
  HWLOC_BACKEND_SYSFS,
#endif
#ifdef HWLOC_HAVE_XML
  HWLOC_BACKEND_XML,
#endif
  /* This value is only here so that we can end the enum list without
     a comma (thereby preventing compiler warnings) */
  HWLOC_BACKEND_MAX
} hwloc_backend_t;

struct hwloc_topology {
  unsigned nb_levels;					/* Number of horizontal levels */
  unsigned next_group_depth;				/* Depth of the next Group object that we may create */
  unsigned level_nbobjects[HWLOC_DEPTH_MAX]; 		/* Number of objects on each horizontal level */
  struct hwloc_obj **levels[HWLOC_DEPTH_MAX];		/* Direct access to levels, levels[l = 0 .. nblevels-1][0..level_nbobjects[l]] */
  unsigned long flags;
  int type_depth[HWLOC_OBJ_TYPE_MAX];
  enum hwloc_ignore_type_e ignored_types[HWLOC_OBJ_TYPE_MAX];
  int is_thissystem;
  int is_loaded;
  hwloc_pid_t pid;                                      /* Process ID the topology is view from, 0 for self */

  int (*set_thisproc_cpubind)(hwloc_topology_t topology, hwloc_const_cpuset_t set, int flags);
  int (*get_thisproc_cpubind)(hwloc_topology_t topology, hwloc_cpuset_t set, int flags);
  int (*set_thisthread_cpubind)(hwloc_topology_t topology, hwloc_const_cpuset_t set, int flags);
  int (*get_thisthread_cpubind)(hwloc_topology_t topology, hwloc_cpuset_t set, int flags);
  int (*set_proc_cpubind)(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_const_cpuset_t set, int flags);
  int (*get_proc_cpubind)(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_cpuset_t set, int flags);
#ifdef hwloc_thread_t
  int (*set_thread_cpubind)(hwloc_topology_t topology, hwloc_thread_t tid, hwloc_const_cpuset_t set, int flags);
  int (*get_thread_cpubind)(hwloc_topology_t topology, hwloc_thread_t tid, hwloc_cpuset_t set, int flags);
#endif

  int (*get_thisproc_last_cpu_location)(hwloc_topology_t topology, hwloc_cpuset_t set, int flags);
  int (*get_thisthread_last_cpu_location)(hwloc_topology_t topology, hwloc_cpuset_t set, int flags);
  int (*get_proc_last_cpu_location)(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_cpuset_t set, int flags);

  int (*set_thisproc_membind)(hwloc_topology_t topology, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags);
  int (*get_thisproc_membind)(hwloc_topology_t topology, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags);
  int (*set_thisthread_membind)(hwloc_topology_t topology, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags);
  int (*get_thisthread_membind)(hwloc_topology_t topology, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags);
  int (*set_proc_membind)(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags);
  int (*get_proc_membind)(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags);
  int (*set_area_membind)(hwloc_topology_t topology, const void *addr, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags);
  int (*get_area_membind)(hwloc_topology_t topology, const void *addr, size_t len, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags);
  /* This has to return the same kind of pointer as alloc_membind, so that free_membind can be used on it */
  void *(*alloc)(hwloc_topology_t topology, size_t len);
  /* alloc_membind has to always succeed if !(flags & HWLOC_MEMBIND_STRICT).
   * see hwloc_alloc_or_fail which is convenient for that.  */
  void *(*alloc_membind)(hwloc_topology_t topology, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags);
  int (*free_membind)(hwloc_topology_t topology, void *addr, size_t len);

  struct hwloc_topology_support support;

  struct hwloc_os_distances_s {
    int nbobjs;
    unsigned *indexes; /* array of OS indexes before we can convert them into objs. always available.
			*/
    struct hwloc_obj **objs; /* array of objects, in the same order as above.
			      * either given (by a backend) together with the indexes array above.
			      * or build from the above indexes array when not given (by the user).
			      */
    float *distances; /* distance matrices, ordered according to the above indexes/objs array.
		       * distance from i to j is stored in slot i*nbnodes+j.
		       * will be copied into the main logical-index-ordered distance at the end of the discovery.
		       */
  } os_distances[HWLOC_OBJ_TYPE_MAX];

  hwloc_backend_t backend_type;
  union hwloc_backend_params_u {
#ifdef HWLOC_LINUX_SYS
    struct hwloc_backend_params_sysfs_s {
      /* sysfs backend parameters */
      char *root_path; /* The path of the file system root, used when browsing, e.g., Linux' sysfs and procfs. */
      int root_fd; /* The file descriptor for the file system root, used when browsing, e.g., Linux' sysfs and procfs. */
    } sysfs;
#endif /* HWLOC_LINUX_SYS */
#if defined(HWLOC_OSF_SYS) || defined(HWLOC_COMPILE_PORTS)
    struct hwloc_backend_params_osf {
      int nbnodes;
    } osf;
#endif /* HWLOC_OSF_SYS */
#ifdef HWLOC_HAVE_XML
    struct hwloc_backend_params_xml_s {
      /* xml backend parameters */
      void *doc;
    } xml;
#endif /* HWLOC_HAVE_XML */
    struct hwloc_backend_params_synthetic_s {
      /* synthetic backend parameters */
#define HWLOC_SYNTHETIC_MAX_DEPTH 128
      unsigned arity[HWLOC_SYNTHETIC_MAX_DEPTH];
      hwloc_obj_type_t type[HWLOC_SYNTHETIC_MAX_DEPTH];
      unsigned id[HWLOC_SYNTHETIC_MAX_DEPTH];
      unsigned depth[HWLOC_SYNTHETIC_MAX_DEPTH]; /* For cache/misc */
    } synthetic;
  } backend_params;
};


extern void hwloc_setup_pu_level(struct hwloc_topology *topology, unsigned nb_pus);
extern int hwloc_get_sysctlbyname(const char *name, int64_t *n);
extern int hwloc_get_sysctl(int name[], unsigned namelen, int *n);
extern unsigned hwloc_fallback_nbprocessors(struct hwloc_topology *topology);

#if defined(HWLOC_LINUX_SYS)
extern void hwloc_look_linux(struct hwloc_topology *topology);
extern void hwloc_set_linux_hooks(struct hwloc_topology *topology);
extern int hwloc_backend_sysfs_init(struct hwloc_topology *topology, const char *fsroot_path);
extern void hwloc_backend_sysfs_exit(struct hwloc_topology *topology);
#endif /* HWLOC_LINUX_SYS */

#ifdef HWLOC_HAVE_XML
extern int hwloc_backend_xml_init(struct hwloc_topology *topology, const char *xmlpath, const char *xmlbuffer, int buflen);
extern void hwloc_xml_check_distances(struct hwloc_topology *topology);
extern void hwloc_look_xml(struct hwloc_topology *topology);
extern void hwloc_backend_xml_exit(struct hwloc_topology *topology);
#endif /* HWLOC_HAVE_XML */

#ifdef HWLOC_SOLARIS_SYS
extern void hwloc_look_solaris(struct hwloc_topology *topology);
extern void hwloc_set_solaris_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_SOLARIS_SYS */

#ifdef HWLOC_AIX_SYS
extern void hwloc_look_aix(struct hwloc_topology *topology);
extern void hwloc_set_aix_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_AIX_SYS */

#ifdef HWLOC_OSF_SYS
extern void hwloc_look_osf(struct hwloc_topology *topology);
extern void hwloc_set_osf_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_OSF_SYS */

#ifdef HWLOC_WIN_SYS
extern void hwloc_look_windows(struct hwloc_topology *topology);
extern void hwloc_set_windows_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_WIN_SYS */

#ifdef HWLOC_DARWIN_SYS
extern void hwloc_look_darwin(struct hwloc_topology *topology);
extern void hwloc_set_darwin_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_DARWIN_SYS */

#ifdef HWLOC_FREEBSD_SYS
extern void hwloc_look_freebsd(struct hwloc_topology *topology);
extern void hwloc_set_freebsd_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_FREEBSD_SYS */

#ifdef HWLOC_HPUX_SYS
extern void hwloc_look_hpux(struct hwloc_topology *topology);
extern void hwloc_set_hpux_hooks(struct hwloc_topology *topology);
#endif /* HWLOC_HPUX_SYS */

extern void hwloc_look_x86(struct hwloc_topology *topology, unsigned nbprocs);

extern int hwloc_backend_synthetic_init(struct hwloc_topology *topology, const char *description);
extern void hwloc_backend_synthetic_exit(struct hwloc_topology *topology);
extern void hwloc_look_synthetic (struct hwloc_topology *topology);

/*
 * Add an object to the topology.
 * It is sorted along the tree of other objects according to the inclusion of
 * cpusets, to eventually be added as a child of the smallest object including
 * this object.
 *
 * If the cpuset is empty, the type of the object (and maybe some attributes)
 * must be enough to find where to insert the object. This is especially true
 * for NUMA nodes with memory and no CPUs.
 *
 * The given object should not have children.
 *
 * This shall only be called before levels are built.
 *
 * In case of error, hwloc_report_os_error() is called.
 */
extern void hwloc_insert_object_by_cpuset(struct hwloc_topology *topology, hwloc_obj_t obj);

/*
 * Add an object to the topology and specify which error callback to use
 */
typedef void (*hwloc_report_error_t)(const char * msg, int line);
extern void hwloc_report_os_error(const char * msg, int line);
extern int hwloc__insert_object_by_cpuset(struct hwloc_topology *topology, hwloc_obj_t obj, hwloc_report_error_t report_error);

/*
 * Insert an object somewhere in the topology.
 *
 * It is added as the last child of the given parent.
 * The cpuset is completely ignored, so strange objects such as I/O devices should
 * preferably be inserted with this.
 *
 * The given object may have children.
 *
 * Remember to call topology_connect() afterwards to fix handy pointers.
 */
extern void hwloc_insert_object_by_parent(struct hwloc_topology *topology, hwloc_obj_t parent, hwloc_obj_t obj);

/* Insert name/value in the object infos array. name and value are copied by the callee. */
extern void hwloc_add_object_info(hwloc_obj_t obj, const char *name, const char *value);

/* Insert uname-specific names/values in the object infos array */
extern void hwloc_add_uname_info(struct hwloc_topology *topology);

/** \brief Return a locally-allocated stringified bitmap for printf-like calls. */
static __hwloc_inline char *
hwloc_bitmap_printf_value(hwloc_const_bitmap_t bitmap)
{
  char *buf;
  hwloc_bitmap_asprintf(&buf, bitmap);
  return buf;
}

static __hwloc_inline struct hwloc_obj *
hwloc_alloc_setup_object(hwloc_obj_type_t type, signed idx)
{
  struct hwloc_obj *obj = malloc(sizeof(*obj));
  memset(obj, 0, sizeof(*obj));
  obj->type = type;
  obj->os_index = idx;
  obj->os_level = -1;
  obj->attr = malloc(sizeof(*obj->attr));
  memset(obj->attr, 0, sizeof(*obj->attr));
  /* do not allocate the cpuset here, let the caller do it */
  return obj;
}

extern void hwloc_free_unlinked_object(hwloc_obj_t obj);

#define hwloc_object_cpuset_from_array(l, _value, _array, _max) do {	\
		struct hwloc_obj *__l = (l);				\
		unsigned int *__a = (_array);				\
		int k;							\
		__l->cpuset = hwloc_bitmap_alloc();			\
		for(k=0; k<_max; k++)					\
			if (__a[k] == _value)				\
				hwloc_bitmap_set(__l->cpuset, k);	\
	} while (0)

/* Configures an array of NUM objects of type TYPE with physical IDs OSPHYSIDS
 * and for which processors have ID PROC_PHYSIDS, and add them to the topology.
 * */
static __hwloc_inline void
hwloc_setup_level(int procid_max, unsigned num, unsigned *osphysids, unsigned *proc_physids, struct hwloc_topology *topology, hwloc_obj_type_t type)
{
  struct hwloc_obj *obj;
  unsigned j;

  hwloc_debug("%d %s\n", num, hwloc_obj_type_string(type));

  for (j = 0; j < num; j++)
    {
      obj = hwloc_alloc_setup_object(type, osphysids[j]);
      hwloc_object_cpuset_from_array(obj, j, proc_physids, procid_max);
      hwloc_debug_2args_bitmap("%s %d has cpuset %s\n",
		 hwloc_obj_type_string(type),
		 j, obj->cpuset);
      hwloc_insert_object_by_cpuset(topology, obj);
    }
  hwloc_debug("%s", "\n");
}

/* This can be used for the alloc field to get allocated data that can be freed by free() */
void *hwloc_alloc_heap(hwloc_topology_t topology, size_t len);

/* This can be used for the alloc field to get allocated data that can be freed by munmap() */
void *hwloc_alloc_mmap(hwloc_topology_t topology, size_t len);

/* This can be used for the free_membind field to free data using free() */
int hwloc_free_heap(hwloc_topology_t topology, void *addr, size_t len);

/* This can be used for the free_membind field to free data using munmap() */
int hwloc_free_mmap(hwloc_topology_t topology, void *addr, size_t len);

/* Allocates unbound memory or fail, depending on whether STRICT is requested
 * or not */
static __hwloc_inline void *
hwloc_alloc_or_fail(hwloc_topology_t topology, size_t len, int flags)
{
  if (flags & HWLOC_MEMBIND_STRICT)
    return NULL;
  return hwloc_alloc(topology, len);
}

extern void hwloc_topology_distances_init(struct hwloc_topology *topology);
extern void hwloc_topology_distances_clear(struct hwloc_topology *topology);
extern void hwloc_topology_distances_destroy(struct hwloc_topology *topology);
extern void hwloc_topology__set_distance_matrix(struct hwloc_topology *topology, hwloc_obj_type_t type, unsigned nbobjs, unsigned *indexes, hwloc_obj_t *objs, float *distances);
extern void hwloc_store_distances_from_env(struct hwloc_topology *topology);
extern void hwloc_convert_distances_indexes_into_objects(struct hwloc_topology *topology);
extern void hwloc_finalize_logical_distances(struct hwloc_topology *topology);
extern void hwloc_restrict_distances(struct hwloc_topology *topology, unsigned long flags);
extern void hwloc_free_logical_distances(struct hwloc_distances_s *dist);
extern void hwloc_group_by_distances(struct hwloc_topology *topology);

#endif /* HWLOC_PRIVATE_H */
