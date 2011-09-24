/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2010 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief High-level hwloc traversal helpers.
 */

#ifndef HWLOC_HELPER_H
#define HWLOC_HELPER_H

#ifndef HWLOC_H
#error Please include the main hwloc.h instead
#endif

#include <stdlib.h>
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup hwlocality_helper_types Object Type Helpers
 * @{
 */

/** \brief Returns the depth of objects of type \p type or below
 *
 * If no object of this type is present on the underlying architecture, the
 * function returns the depth of the first "present" object typically found
 * inside \p type.
 */
static __hwloc_inline int __hwloc_attribute_pure
hwloc_get_type_or_below_depth (hwloc_topology_t topology, hwloc_obj_type_t type)
{
  int depth = hwloc_get_type_depth(topology, type);

  if (depth != HWLOC_TYPE_DEPTH_UNKNOWN)
    return depth;

  /* find the highest existing level with type order >= */
  for(depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PU); ; depth--)
    if (hwloc_compare_types(hwloc_get_depth_type(topology, depth), type) < 0)
      return depth+1;

  /* Shouldn't ever happen, as there is always a SYSTEM level with lower order and known depth.  */
  /* abort(); */
}

/** \brief Returns the depth of objects of type \p type or above
 *
 * If no object of this type is present on the underlying architecture, the
 * function returns the depth of the first "present" object typically
 * containing \p type.
 */
static __hwloc_inline int __hwloc_attribute_pure
hwloc_get_type_or_above_depth (hwloc_topology_t topology, hwloc_obj_type_t type)
{
  int depth = hwloc_get_type_depth(topology, type);

  if (depth != HWLOC_TYPE_DEPTH_UNKNOWN)
    return depth;

  /* find the lowest existing level with type order <= */
  for(depth = 0; ; depth++)
    if (hwloc_compare_types(hwloc_get_depth_type(topology, depth), type) > 0)
      return depth-1;

  /* Shouldn't ever happen, as there is always a PU level with higher order and known depth.  */
  /* abort(); */
}

/** @} */



/** \defgroup hwlocality_helper_traversal_basic Basic Traversal Helpers
 * @{
 */

/** \brief Returns the top-object of the topology-tree.
 *
 * Its type is typically ::HWLOC_OBJ_MACHINE but it could be different
 * for complex topologies.  This function replaces the old deprecated
 * hwloc_get_system_obj().
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_root_obj (hwloc_topology_t topology)
{
  return hwloc_get_obj_by_depth (topology, 0, 0);
}

/** \brief Returns the ancestor object of \p obj at depth \p depth. */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_ancestor_obj_by_depth (hwloc_topology_t topology __hwloc_attribute_unused, unsigned depth, hwloc_obj_t obj)
{
  hwloc_obj_t ancestor = obj;
  if (obj->depth < depth)
    return NULL;
  while (ancestor && ancestor->depth > depth)
    ancestor = ancestor->parent;
  return ancestor;
}

/** \brief Returns the ancestor object of \p obj with type \p type. */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_ancestor_obj_by_type (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_obj_type_t type, hwloc_obj_t obj)
{
  hwloc_obj_t ancestor = obj->parent;
  while (ancestor && ancestor->type != type)
    ancestor = ancestor->parent;
  return ancestor;
}

/** \brief Returns the next object at depth \p depth.
 *
 * If \p prev is \c NULL, return the first object at depth \p depth.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_by_depth (hwloc_topology_t topology, unsigned depth, hwloc_obj_t prev)
{
  if (!prev)
    return hwloc_get_obj_by_depth (topology, depth, 0);
  if (prev->depth != depth)
    return NULL;
  return prev->next_cousin;
}

/** \brief Returns the next object of type \p type.
 *
 * If \p prev is \c NULL, return the first object at type \p type.  If
 * there are multiple or no depth for given type, return \c NULL and
 * let the caller fallback to hwloc_get_next_obj_by_depth().
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_by_type (hwloc_topology_t topology, hwloc_obj_type_t type,
		   hwloc_obj_t prev)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE)
    return NULL;
  return hwloc_get_next_obj_by_depth (topology, depth, prev);
}

/** \brief Returns the object of type ::HWLOC_OBJ_PU with \p os_index.
 *
 * \note The \p os_index field of object should most of the times only be
 * used for pretty-printing purpose. Type ::HWLOC_OBJ_PU is the only case
 * where \p os_index could actually be useful, when manually binding to
 * processors.
 * However, using CPU sets to hide this complexity should often be preferred.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_pu_obj_by_os_index(hwloc_topology_t topology, unsigned os_index)
{
  hwloc_obj_t obj = NULL;
  while ((obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_PU, obj)) != NULL)
    if (obj->os_index == os_index)
      return obj;
  return NULL;
}

/** \brief Return the next child.
 *
 * If \p prev is \c NULL, return the first child.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_child (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_obj_t parent, hwloc_obj_t prev)
{
  if (!prev)
    return parent->first_child;
  if (prev->parent != parent)
    return NULL;
  return prev->next_sibling;
}

/** \brief Returns the common parent object to objects lvl1 and lvl2 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_common_ancestor_obj (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_obj_t obj1, hwloc_obj_t obj2)
{
  /* the loop isn't so easy since intermediate ancestors may have
   * different depth, causing us to alternate between using obj1->parent
   * and obj2->parent. Also, even if at some point we find ancestors of
   * of the same depth, their ancestors may have different depth again.
   */
  while (obj1 != obj2) {
    while (obj1->depth > obj2->depth)
      obj1 = obj1->parent;
    while (obj2->depth > obj1->depth)
      obj2 = obj2->parent;
    if (obj1 != obj2 && obj1->depth == obj2->depth) {
      obj1 = obj1->parent;
      obj2 = obj2->parent;
    }
  }
  return obj1;
}

/** \brief Returns true if \p obj is inside the subtree beginning with \p subtree_root.
 *
 * \note This function assumes that both \p obj and \p subtree_root have a \p cpuset.
 */
static __hwloc_inline int __hwloc_attribute_pure
hwloc_obj_is_in_subtree (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_obj_t obj, hwloc_obj_t subtree_root)
{
  return hwloc_bitmap_isincluded(obj->cpuset, subtree_root->cpuset);
}

/** @} */



/** \defgroup hwlocality_helper_find_inside Finding Objects Inside a CPU set
 * @{
 */

/** \brief Get the first largest object included in the given cpuset \p set.
 *
 * \return the first object that is included in \p set and whose parent is not.
 *
 * This is convenient for iterating over all largest objects within a CPU set
 * by doing a loop getting the first largest object and clearing its CPU set
 * from the remaining CPU set.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_first_largest_obj_inside_cpuset(hwloc_topology_t topology, hwloc_const_cpuset_t set)
{
  hwloc_obj_t obj = hwloc_get_root_obj(topology);
  /* FIXME: what if !root->cpuset? */
  if (!hwloc_bitmap_intersects(obj->cpuset, set))
    return NULL;
  while (!hwloc_bitmap_isincluded(obj->cpuset, set)) {
    /* while the object intersects without being included, look at its children */
    hwloc_obj_t child = NULL;
    while ((child = hwloc_get_next_child(topology, obj, child)) != NULL) {
      if (child->cpuset && hwloc_bitmap_intersects(child->cpuset, set))
	break;
    }
    if (!child)
      /* no child intersects, return their father */
      return obj;
    /* found one intersecting child, look at its children */
    obj = child;
  }
  /* obj is included, return it */
  return obj;
}

/** \brief Get the set of largest objects covering exactly a given cpuset \p set
 *
 * \return the number of objects returned in \p objs.
 */
HWLOC_DECLSPEC int hwloc_get_largest_objs_inside_cpuset (hwloc_topology_t topology, hwloc_const_cpuset_t set,
						 hwloc_obj_t * __hwloc_restrict objs, int max);

/** \brief Return the next object at depth \p depth included in CPU set \p set.
 *
 * If \p prev is \c NULL, return the first object at depth \p depth
 * included in \p set.  The next invokation should pass the previous
 * return value in \p prev so as to obtain the next object in \p set.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_inside_cpuset_by_depth (hwloc_topology_t topology, hwloc_const_cpuset_t set,
					   unsigned depth, hwloc_obj_t prev)
{
  hwloc_obj_t next = hwloc_get_next_obj_by_depth(topology, depth, prev);
  /* no need to check next->cpuset because objects in levels always have a cpuset */
  while (next && !hwloc_bitmap_isincluded(next->cpuset, set))
    next = next->next_cousin;
  return next;
}

/** \brief Return the next object of type \p type included in CPU set \p set.
 *
 * If there are multiple or no depth for given type, return \c NULL
 * and let the caller fallback to
 * hwloc_get_next_obj_inside_cpuset_by_depth().
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_inside_cpuset_by_type (hwloc_topology_t topology, hwloc_const_cpuset_t set,
					  hwloc_obj_type_t type, hwloc_obj_t prev)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE)
    return NULL;
  return hwloc_get_next_obj_inside_cpuset_by_depth(topology, set, depth, prev);
}

/** \brief Return the (logically) \p idx -th object at depth \p depth included in CPU set \p set.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_obj_inside_cpuset_by_depth (hwloc_topology_t topology, hwloc_const_cpuset_t set,
				      unsigned depth, unsigned idx)
{
  unsigned count = 0;
  hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, depth, 0);
  while (obj) {
    /* no need to check obj->cpuset because objects in levels always have a cpuset */
    if (hwloc_bitmap_isincluded(obj->cpuset, set)) {
      if (count == idx)
	return obj;
      count++;
    }
    obj = obj->next_cousin;
  }
  return NULL;
}

/** \brief Return the \p idx -th object of type \p type included in CPU set \p set.
 *
 * If there are multiple or no depth for given type, return \c NULL
 * and let the caller fallback to
 * hwloc_get_obj_inside_cpuset_by_depth().
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_obj_inside_cpuset_by_type (hwloc_topology_t topology, hwloc_const_cpuset_t set,
				     hwloc_obj_type_t type, unsigned idx)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE)
    return NULL;
  return hwloc_get_obj_inside_cpuset_by_depth(topology, set, depth, idx);
}

/** \brief Return the number of objects at depth \p depth included in CPU set \p set. */
static __hwloc_inline unsigned __hwloc_attribute_pure
hwloc_get_nbobjs_inside_cpuset_by_depth (hwloc_topology_t topology, hwloc_const_cpuset_t set,
					 unsigned depth)
{
  hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, depth, 0);
  int count = 0;
  while (obj) {
    /* no need to check obj->cpuset because objects in levels always have a cpuset */
    if (hwloc_bitmap_isincluded(obj->cpuset, set))
      count++;
    obj = obj->next_cousin;
  }
  return count;
}

/** \brief Return the number of objects of type \p type included in CPU set \p set.
 *
 * If no object for that type exists inside CPU set \p set, 0 is
 * returned.  If there are several levels with objects of that type
 * inside CPU set \p set, -1 is returned.
 */
static __hwloc_inline int __hwloc_attribute_pure
hwloc_get_nbobjs_inside_cpuset_by_type (hwloc_topology_t topology, hwloc_const_cpuset_t set,
					hwloc_obj_type_t type)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN)
    return 0;
  if (depth == HWLOC_TYPE_DEPTH_MULTIPLE)
    return -1; /* FIXME: agregate nbobjs from different levels? */
  return hwloc_get_nbobjs_inside_cpuset_by_depth(topology, set, depth);
}

/** @} */



/** \defgroup hwlocality_helper_find_covering Finding a single Object covering at least CPU set
 * @{
 */

/** \brief Get the child covering at least CPU set \p set.
 *
 * \return \c NULL if no child matches or if \p set is empty.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_child_covering_cpuset (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_const_cpuset_t set,
				hwloc_obj_t parent)
{
  hwloc_obj_t child;

  if (hwloc_bitmap_iszero(set))
    return NULL;

  child = parent->first_child;
  while (child) {
    if (child->cpuset && hwloc_bitmap_isincluded(set, child->cpuset))
      return child;
    child = child->next_sibling;
  }
  return NULL;
}

/** \brief Get the lowest object covering at least CPU set \p set
 *
 * \return \c NULL if no object matches or if \p set is empty.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_obj_covering_cpuset (hwloc_topology_t topology, hwloc_const_cpuset_t set)
{
  struct hwloc_obj *current = hwloc_get_root_obj(topology);

  if (hwloc_bitmap_iszero(set))
    return NULL;

  /* FIXME: what if !root->cpuset? */
  if (!hwloc_bitmap_isincluded(set, current->cpuset))
    return NULL;

  while (1) {
    hwloc_obj_t child = hwloc_get_child_covering_cpuset(topology, set, current);
    if (!child)
      return current;
    current = child;
  }
}


/** @} */



/** \defgroup hwlocality_helper_find_coverings Finding a set of similar Objects covering at least a CPU set
 * @{
 */

/** \brief Iterate through same-depth objects covering at least CPU set \p set
 *
 * If object \p prev is \c NULL, return the first object at depth \p
 * depth covering at least part of CPU set \p set.  The next
 * invokation should pass the previous return value in \p prev so as
 * to obtain the next object covering at least another part of \p set.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_covering_cpuset_by_depth(hwloc_topology_t topology, hwloc_const_cpuset_t set,
					    unsigned depth, hwloc_obj_t prev)
{
  hwloc_obj_t next = hwloc_get_next_obj_by_depth(topology, depth, prev);
  /* no need to check next->cpuset because objects in levels always have a cpuset */
  while (next && !hwloc_bitmap_intersects(set, next->cpuset))
    next = next->next_cousin;
  return next;
}

/** \brief Iterate through same-type objects covering at least CPU set \p set
 *
 * If object \p prev is \c NULL, return the first object of type \p
 * type covering at least part of CPU set \p set.  The next invokation
 * should pass the previous return value in \p prev so as to obtain
 * the next object of type \p type covering at least another part of
 * \p set.
 *
 * If there are no or multiple depths for type \p type, \c NULL is returned.
 * The caller may fallback to hwloc_get_next_obj_covering_cpuset_by_depth()
 * for each depth.
 */
static __hwloc_inline hwloc_obj_t
hwloc_get_next_obj_covering_cpuset_by_type(hwloc_topology_t topology, hwloc_const_cpuset_t set,
					   hwloc_obj_type_t type, hwloc_obj_t prev)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE)
    return NULL;
  return hwloc_get_next_obj_covering_cpuset_by_depth(topology, set, depth, prev);
}

/** @} */



/** \defgroup hwlocality_helper_find_cache Cache-specific Finding Helpers
 * @{
 */

/** \brief Get the first cache covering a cpuset \p set
 *
 * \return \c NULL if no cache matches
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_cache_covering_cpuset (hwloc_topology_t topology, hwloc_const_cpuset_t set)
{
  hwloc_obj_t current = hwloc_get_obj_covering_cpuset(topology, set);
  while (current) {
    if (current->type == HWLOC_OBJ_CACHE)
      return current;
    current = current->parent;
  }
  return NULL;
}

/** \brief Get the first cache shared between an object and somebody else.
 *
 * \return \c NULL if no cache matches or if an invalid object is given.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_shared_cache_covering_obj (hwloc_topology_t topology __hwloc_attribute_unused, hwloc_obj_t obj)
{
  hwloc_obj_t current = obj->parent;
  if (!obj->cpuset)
    return NULL;
  while (current && current->cpuset) {
    if (!hwloc_bitmap_isequal(current->cpuset, obj->cpuset)
        && current->type == HWLOC_OBJ_CACHE)
      return current;
    current = current->parent;
  }
  return NULL;
}

/** @} */



/** \defgroup hwlocality_helper_traversal Advanced Traversal Helpers
 * @{
 */

/** \brief Do a depth-first traversal of the topology to find and sort
 *
 *  all objects that are at the same depth than \p src.
 *  Report in \p objs up to \p max physically closest ones to \p src.
 *
 *  \return the number of objects returned in \p objs.
 */
/* TODO: rather provide an iterator? Provide a way to know how much should be allocated? By returning the total number of objects instead? */
HWLOC_DECLSPEC unsigned hwloc_get_closest_objs (hwloc_topology_t topology, hwloc_obj_t src, hwloc_obj_t * __hwloc_restrict objs, unsigned max);

/** \brief Find an object below another object, both specified by types and indexes.
 *
 * Start from the top system object and find object of type \p type1
 * and logical index \p idx1.  Then look below this object and find another
 * object of type \p type2 and logical index \p idx2.  Indexes are specified
 * within the parent, not withing the entire system.
 *
 * For instance, if type1 is SOCKET, idx1 is 2, type2 is CORE and idx2
 * is 3, return the fourth core object below the third socket.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_obj_below_by_type (hwloc_topology_t topology,
			     hwloc_obj_type_t type1, unsigned idx1,
			     hwloc_obj_type_t type2, unsigned idx2)
{
  hwloc_obj_t obj;

  obj = hwloc_get_obj_by_type (topology, type1, idx1);
  if (!obj)
    return NULL;

  return hwloc_get_obj_inside_cpuset_by_type(topology, obj->cpuset, type2, idx2);
}

/** \brief Find an object below a chain of objects specified by types and indexes.
 *
 * This is a generalized version of hwloc_get_obj_below_by_type().
 *
 * Arrays \p typev and \p idxv must contain \p nr types and indexes.
 *
 * Start from the top system object and walk the arrays \p typev and \p idxv.
 * For each type and logical index couple in the arrays, look under the previously found
 * object to find the index-th object of the given type.
 * Indexes are specified within the parent, not withing the entire system.
 *
 * For instance, if nr is 3, typev contains NODE, SOCKET and CORE,
 * and idxv contains 0, 1 and 2, return the third core object below
 * the second socket below the first NUMA node.
 */
static __hwloc_inline hwloc_obj_t __hwloc_attribute_pure
hwloc_get_obj_below_array_by_type (hwloc_topology_t topology, int nr, hwloc_obj_type_t *typev, unsigned *idxv)
{
  hwloc_obj_t obj = hwloc_get_root_obj(topology);
  int i;

  /* FIXME: what if !root->cpuset? */
  for(i=0; i<nr; i++) {
    obj = hwloc_get_obj_inside_cpuset_by_type(topology, obj->cpuset, typev[i], idxv[i]);
    if (!obj)
      return NULL;
  }

  return obj;
}

/** @} */



/** \defgroup hwlocality_helper_binding Binding Helpers
 * @{
 */

/** \brief Distribute \p n items over the topology under \p root
 *
 * Array \p cpuset will be filled with \p n cpusets recursively distributed
 * linearly over the topology under \p root, down to depth \p until (which can
 * be INT_MAX to distribute down to the finest level).
 *
 * This is typically useful when an application wants to distribute \p n
 * threads over a machine, giving each of them as much private cache as
 * possible and keeping them locally in number order.
 *
 * The caller may typically want to also call hwloc_bitmap_singlify()
 * before binding a thread so that it does not move at all.
 */
static __hwloc_inline void
hwloc_distributev(hwloc_topology_t topology, hwloc_obj_t *root, unsigned n_roots, hwloc_cpuset_t *cpuset, unsigned n, unsigned until);
static __hwloc_inline void
hwloc_distribute(hwloc_topology_t topology, hwloc_obj_t root, hwloc_cpuset_t *cpuset, unsigned n, unsigned until)
{
  unsigned i;

  /* FIXME: what if !root->cpuset? */
  if (!root->arity || n == 1 || root->depth >= until) {
    /* Got to the bottom, we can't split any more, put everything there.  */
    for (i=0; i<n; i++)
      cpuset[i] = hwloc_bitmap_dup(root->cpuset);
    return;
  }

  hwloc_distributev(topology, root->children, root->arity, cpuset, n, until);
}

/** \brief Distribute \p n items over the topology under \p roots
 *
 * This is the same as hwloc_distribute, but takes an array of roots instead of
 * just one root.
 */
static __hwloc_inline void
hwloc_distributev(hwloc_topology_t topology, hwloc_obj_t *roots, unsigned n_roots, hwloc_cpuset_t *cpuset, unsigned n, unsigned until)
{
  unsigned i;
  unsigned tot_weight;
  hwloc_cpuset_t *cpusetp = cpuset;

  tot_weight = 0;
  for (i = 0; i < n_roots; i++)
    if (roots[i]->cpuset)
      tot_weight += hwloc_bitmap_weight(roots[i]->cpuset);

  for (i = 0; i < n_roots && tot_weight; i++) {
    /* Give to roots[i] a portion proportional to its weight */
    unsigned weight = roots[i]->cpuset ? hwloc_bitmap_weight(roots[i]->cpuset) : 0;
    unsigned chunk = (n * weight + tot_weight-1) / tot_weight;
    hwloc_distribute(topology, roots[i], cpusetp, chunk, until);
    cpusetp += chunk;
    tot_weight -= weight;
    n -= chunk;
  }
}

/** \brief Allocate some memory on the given nodeset \p nodeset
 *
 * This is similar to hwloc_alloc_membind except that it is allowed to change
 * the current memory binding policy, thus providing more binding support, at
 * the expense of changing the current state.
 */
static __hwloc_inline void *
hwloc_alloc_membind_policy_nodeset(hwloc_topology_t topology, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  void *p = hwloc_alloc_membind_nodeset(topology, len, nodeset, policy, flags);
  if (p)
    return p;
  hwloc_set_membind_nodeset(topology, nodeset, policy, flags);
  p = hwloc_alloc(topology, len);
  if (p && policy != HWLOC_MEMBIND_FIRSTTOUCH)
    /* Enforce the binding by touching the data */
    memset(p, 0, len);
  return p;
}

/** \brief Allocate some memory on the memory nodes near given cpuset \p cpuset
 *
 * This is similar to hwloc_alloc_membind_policy_nodeset, but for a given cpuset.
 */
static __hwloc_inline void *
hwloc_alloc_membind_policy(hwloc_topology_t topology, size_t len, hwloc_const_cpuset_t cpuset, hwloc_membind_policy_t policy, int flags)
{
  void *p = hwloc_alloc_membind(topology, len, cpuset, policy, flags);
  if (p)
    return p;
  hwloc_set_membind(topology, cpuset, policy, flags);
  p = hwloc_alloc(topology, len);
  if (p && policy != HWLOC_MEMBIND_FIRSTTOUCH)
    /* Enforce the binding by touching the data */
    memset(p, 0, len);
  return p;
}

/** @} */



/** \defgroup hwlocality_helper_cpuset Cpuset Helpers
 * @{
 */
/** \brief Get complete CPU set
 *
 * \return the complete CPU set of logical processors of the system. If the
 * topology is the result of a combination of several systems, NULL is
 * returned.
 *
 * \note The returned cpuset is not newly allocated and should thus not be
 * changed or freed; hwloc_cpuset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_cpuset_t __hwloc_attribute_pure
hwloc_topology_get_complete_cpuset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->complete_cpuset;
}

/** \brief Get topology CPU set
 *
 * \return the CPU set of logical processors of the system for which hwloc
 * provides topology information. This is equivalent to the cpuset of the
 * system object. If the topology is the result of a combination of several
 * systems, NULL is returned.
 *
 * \note The returned cpuset is not newly allocated and should thus not be
 * changed or freed; hwloc_cpuset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_cpuset_t __hwloc_attribute_pure
hwloc_topology_get_topology_cpuset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->cpuset;
}

/** \brief Get online CPU set
 *
 * \return the CPU set of online logical processors of the system. If the
 * topology is the result of a combination of several systems, NULL is
 * returned.
 *
 * \note The returned cpuset is not newly allocated and should thus not be
 * changed or freed; hwloc_cpuset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_cpuset_t __hwloc_attribute_pure
hwloc_topology_get_online_cpuset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->online_cpuset;
}

/** \brief Get allowed CPU set
 *
 * \return the CPU set of allowed logical processors of the system. If the
 * topology is the result of a combination of several systems, NULL is
 * returned.
 *
 * \note The returned cpuset is not newly allocated and should thus not be
 * changed or freed, hwloc_cpuset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_cpuset_t __hwloc_attribute_pure
hwloc_topology_get_allowed_cpuset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->allowed_cpuset;
}

/** @} */



/** \defgroup hwlocality_helper_nodeset Nodeset Helpers
 * @{
 */
/** \brief Get complete node set
 *
 * \return the complete node set of memory of the system. If the
 * topology is the result of a combination of several systems, NULL is
 * returned.
 *
 * \note The returned nodeset is not newly allocated and should thus not be
 * changed or freed; hwloc_nodeset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_nodeset_t __hwloc_attribute_pure
hwloc_topology_get_complete_nodeset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->complete_nodeset;
}

/** \brief Get topology node set
 *
 * \return the node set of memory of the system for which hwloc
 * provides topology information. This is equivalent to the nodeset of the
 * system object. If the topology is the result of a combination of several
 * systems, NULL is returned.
 *
 * \note The returned nodeset is not newly allocated and should thus not be
 * changed or freed; hwloc_nodeset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_nodeset_t __hwloc_attribute_pure
hwloc_topology_get_topology_nodeset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->nodeset;
}

/** \brief Get allowed node set
 *
 * \return the node set of allowed memory of the system. If the
 * topology is the result of a combination of several systems, NULL is
 * returned.
 *
 * \note The returned nodeset is not newly allocated and should thus not be
 * changed or freed, hwloc_nodeset_dup must be used to obtain a local copy.
 */
static __hwloc_inline hwloc_const_nodeset_t __hwloc_attribute_pure
hwloc_topology_get_allowed_nodeset(hwloc_topology_t topology)
{
  return hwloc_get_root_obj(topology)->allowed_nodeset;
}

/** @} */



/** \defgroup hwlocality_helper_nodeset_convert Conversion between cpuset and nodeset 
 *
 * There are two semantics for converting cpusets to nodesets depending on how
 * non-NUMA machines are handled.
 *
 * When manipulating nodesets for memory binding, non-NUMA machines should be
 * considered as having a single NUMA node. The standard conversion routines
 * below should be used so that marking the first bit of the nodeset means
 * that memory should be bound to a non-NUMA whole machine.
 *
 * When manipulating nodesets as an actual list of NUMA nodes without any
 * need to handle memory binding on non-NUMA machines, the strict conversion
 * routines may be used instead.
 * @{
 */

/** \brief Convert a CPU set into a NUMA node set and handle non-NUMA cases
 *
 * If some NUMA nodes have no CPUs at all, this function never sets their
 * indexes in the output node set, even if a full CPU set is given in input.
 *
 * If the topology contains no NUMA nodes, the machine is considered
 * as a single memory node, and the following behavior is used:
 * If \p cpuset is empty, \p nodeset will be emptied as well.
 * Otherwise \p nodeset will be entirely filled.
 */
static __hwloc_inline void
hwloc_cpuset_to_nodeset(hwloc_topology_t topology, hwloc_const_cpuset_t cpuset, hwloc_nodeset_t nodeset)
{
	int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
	hwloc_obj_t obj;

	if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
		 if (hwloc_bitmap_iszero(cpuset))
			hwloc_bitmap_zero(nodeset);
		else
			/* Assume the whole system */
			hwloc_bitmap_fill(nodeset);
		return;
	}

	hwloc_bitmap_zero(nodeset);
	obj = NULL;
	while ((obj = hwloc_get_next_obj_covering_cpuset_by_depth(topology, cpuset, depth, obj)) != NULL)
		hwloc_bitmap_set(nodeset, obj->os_index);
}

/** \brief Convert a CPU set into a NUMA node set without handling non-NUMA cases
 *
 * This is the strict variant of ::hwloc_cpuset_to_nodeset. It does not fix
 * non-NUMA cases. If the topology contains some NUMA nodes, behave exactly
 * the same. However, if the topology contains no NUMA nodes, return an empty
 * nodeset.
 */
static __hwloc_inline void
hwloc_cpuset_to_nodeset_strict(struct hwloc_topology *topology, hwloc_const_cpuset_t cpuset, hwloc_nodeset_t nodeset)
{
	int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
	hwloc_obj_t obj;
	if (depth == HWLOC_TYPE_DEPTH_UNKNOWN )
		return;
	hwloc_bitmap_zero(nodeset);
	obj = NULL;
	while ((obj = hwloc_get_next_obj_covering_cpuset_by_depth(topology, cpuset, depth, obj)) != NULL)
		hwloc_bitmap_set(nodeset, obj->os_index);
}

/** \brief Convert a NUMA node set into a CPU set and handle non-NUMA cases
 *
 * If the topology contains no NUMA nodes, the machine is considered
 * as a single memory node, and the following behavior is used:
 * If \p nodeset is empty, \p cpuset will be emptied as well.
 * Otherwise \p cpuset will be entirely filled.
 * This is useful for manipulating memory binding sets.
 */
static __hwloc_inline void
hwloc_cpuset_from_nodeset(hwloc_topology_t topology, hwloc_cpuset_t cpuset, hwloc_const_nodeset_t nodeset)
{
	int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
	hwloc_obj_t obj;

	if (depth == HWLOC_TYPE_DEPTH_UNKNOWN ) {
		if (hwloc_bitmap_iszero(nodeset))
			hwloc_bitmap_zero(cpuset);
		else
			/* Assume the whole system */
			hwloc_bitmap_fill(cpuset);
		return;
	}

	hwloc_bitmap_zero(cpuset);
	obj = NULL;
	while ((obj = hwloc_get_next_obj_by_depth(topology, depth, obj)) != NULL) {
		if (hwloc_bitmap_isset(nodeset, obj->os_index))
			/* no need to check obj->cpuset because objects in levels always have a cpuset */
			hwloc_bitmap_or(cpuset, cpuset, obj->cpuset);
	}
}

/** \brief Convert a NUMA node set into a CPU set without handling non-NUMA cases
 *
 * This is the strict variant of ::hwloc_cpuset_from_nodeset. It does not fix
 * non-NUMA cases. If the topology contains some NUMA nodes, behave exactly
 * the same. However, if the topology contains no NUMA nodes, return an empty
 * cpuset.
 */
static __hwloc_inline void
hwloc_cpuset_from_nodeset_strict(struct hwloc_topology *topology, hwloc_cpuset_t cpuset, hwloc_const_nodeset_t nodeset)
{
	int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
	hwloc_obj_t obj;
	if (depth == HWLOC_TYPE_DEPTH_UNKNOWN )
		return;
	hwloc_bitmap_zero(cpuset);
	obj = NULL;
	while ((obj = hwloc_get_next_obj_by_depth(topology, depth, obj)) != NULL)
		if (hwloc_bitmap_isset(nodeset, obj->os_index))
			/* no need to check obj->cpuset because objects in levels always have a cpuset */
			hwloc_bitmap_or(cpuset, cpuset, obj->cpuset);
}

/** @} */



/** \defgroup hwlocality_distances Distances
 * @{
 */

/** \brief Get the distances between all objects at the given depth.
 *
 * \return a distances structure containing a matrix with all distances
 * between all objects at the given depth.
 *
 * Slot i+nbobjs*j contains the distance from the object of logical index i
 * the object of logical index j.
 *
 * \note This function only returns matrices covering the whole topology,
 * without any unknown distance value. Those matrices are available in
 * top-level object of the hierarchy. Matrices of lower objects are not
 * reported here since they cover only part of the machine.
 *
 * The returned structure belongs to the hwloc library. The caller should
 * not modify or free it.
 *
 * \return \c NULL if no such distance matrix exists.
 */

static __hwloc_inline const struct hwloc_distances_s *
hwloc_get_whole_distance_matrix_by_depth(hwloc_topology_t topology, unsigned depth)
{
  hwloc_obj_t root = hwloc_get_root_obj(topology);
  unsigned i;
  for(i=0; i<root->distances_count; i++)
    if (root->distances[i]->relative_depth == depth)
      return root->distances[i];
  return NULL;
}

/** \brief Get the distances between all objects of a given type.
 *
 * \return a distances structure containing a matrix with all distances
 * between all objects of the given type.
 *
 * Slot i+nbobjs*j contains the distance from the object of logical index i
 * the object of logical index j.
 *
 * \note This function only returns matrices covering the whole topology,
 * without any unknown distance value. Those matrices are available in
 * top-level object of the hierarchy. Matrices of lower objects are not
 * reported here since they cover only part of the machine.
 *
 * The returned structure belongs to the hwloc library. The caller should
 * not modify or free it.
 *
 * \return \c NULL if no such distance matrix exists.
 */

static __hwloc_inline const struct hwloc_distances_s *
hwloc_get_whole_distance_matrix_by_type(hwloc_topology_t topology, hwloc_obj_type_t type)
{
  int depth = hwloc_get_type_depth(topology, type);
  if (depth < 0)
    return NULL;
  return hwloc_get_whole_distance_matrix_by_depth(topology, depth);
}

/** \brief Get distances for the given depth and covering some objects
 *
 * Return a distance matrix that describes depth \p depth and covers at
 * least object \p obj and all its ancestors.
 *
 * When looking for the distance between some objects, a common ancestor should
 * be passed in \p obj.
 *
 * \p firstp is set to logical index of the first object described by the matrix.
 *
 * The returned structure belongs to the hwloc library. The caller should
 * not modify or free it.
 */
static __hwloc_inline const struct hwloc_distances_s *
hwloc_get_distance_matrix_covering_obj_by_depth(hwloc_topology_t topology,
						hwloc_obj_t obj, unsigned depth,
						unsigned *firstp)
{
  while (obj && obj->cpuset) {
    unsigned i;
    for(i=0; i<obj->distances_count; i++)
      if (obj->distances[i]->relative_depth == depth - obj->depth) {
	if (!obj->distances[i]->nbobjs)
	  continue;
	*firstp = hwloc_get_next_obj_inside_cpuset_by_depth(topology, obj->cpuset, depth, NULL)->logical_index;
	return obj->distances[i];
      }
    obj = obj->parent;
  }
  return NULL;
}

/** \brief Get the latency in both directions between two objects.
 *
 * Look at ancestor objects from the bottom to the top until one of them
 * contains a distance matrix that matches the objects exactly.
 *
 * \p latency gets the value from object \p obj1 to \p obj2, while
 * \p reverse_latency gets the reverse-direction value, which
 * may be different on some architectures.
 *
 * \return -1 if no ancestor contains a matching latency matrix.
 */
static __hwloc_inline int
hwloc_get_latency(hwloc_topology_t topology,
		   hwloc_obj_t obj1, hwloc_obj_t obj2,
		   float *latency, float *reverse_latency)
{
  hwloc_obj_t ancestor;
  const struct hwloc_distances_s * distances;
  unsigned first_logical ;

  if (obj1->depth != obj2->depth) {
    errno = EINVAL;
    return -1;
  }

  ancestor = hwloc_get_common_ancestor_obj(topology, obj1, obj2);
  distances = hwloc_get_distance_matrix_covering_obj_by_depth(topology, ancestor, obj1->depth, &first_logical);
  if (distances && distances->latency) {
    const float * latency_matrix = distances->latency;
    unsigned nbobjs = distances->nbobjs;
    unsigned l1 = obj1->logical_index - first_logical;
    unsigned l2 = obj2->logical_index - first_logical;
    *latency = latency_matrix[l1*nbobjs+l2];
    *reverse_latency = latency_matrix[l2*nbobjs+l1];
    return 0;
  }

  errno = ENOSYS;
  return -1;
}

/** @} */



#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* HWLOC_HELPER_H */
