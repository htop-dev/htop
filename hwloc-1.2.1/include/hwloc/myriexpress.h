/*
 * Copyright © 2010 INRIA.  All rights reserved.
 * Copyright © 2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief Macros to help interaction between hwloc and Myrinet Express.
 *
 * Applications that use both hwloc and Myrinet Express verbs may want to
 * include this file so as to get topology information for Myrinet hardware.
 *
 */

#ifndef HWLOC_MYRIEXPRESS_H
#define HWLOC_MYRIEXPRESS_H

#include <hwloc.h>
#include <hwloc/autogen/config.h>
#include <hwloc/linux.h>

#include <myriexpress.h>


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup hwlocality_myriexpress Myrinet Express-Specific Functions
 * @{
 */

/** \brief Get the CPU set of logical processors that are physically
 * close the MX board \p id.
 *
 * For the given Myrinet Express board index \p id, read the
 * OS-provided NUMA node and return the corresponding CPU set.
 */
static __hwloc_inline int
hwloc_mx_board_get_device_cpuset(hwloc_topology_t topology,
				 unsigned id, hwloc_cpuset_t set)
{
  uint32_t in, out;

  in = id;
  if (mx_get_info(NULL, MX_NUMA_NODE, &in, sizeof(in), &out, sizeof(out)) != MX_SUCCESS) {
    errno = EINVAL;
    return -1;
  }

  if (out != (uint32_t) -1) {
    hwloc_obj_t obj = NULL;
    while ((obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NODE, obj)) != NULL)
      if (obj->os_index == out) {
	hwloc_bitmap_copy(set, obj->cpuset);
	goto out;
      }
  }
  /* fallback to the full topology cpuset */
  hwloc_bitmap_copy(set, hwloc_topology_get_complete_cpuset(topology));

 out:
  return 0;
}

/** \brief Get the CPU set of logical processors that are physically
 * close to endpoint \p endpoint.
 *
 * For the given Myrinet Express endpoint \p endpoint, read the
 * OS-provided NUMA node and return the corresponding CPU set.
 */
static __hwloc_inline int
hwloc_mx_endpoint_get_device_cpuset(hwloc_topology_t topology,
				    mx_endpoint_t endpoint, hwloc_cpuset_t set)
{
  uint64_t nid;
  uint32_t nindex, eid;
  mx_endpoint_addr_t eaddr;

  if (mx_get_endpoint_addr(endpoint, &eaddr) != MX_SUCCESS) {
    errno = EINVAL;
    return -1;
  }

  if (mx_decompose_endpoint_addr(eaddr, &nid, &eid) != MX_SUCCESS) {
    errno = EINVAL;
    return -1;
  }

  if (mx_nic_id_to_board_number(nid, &nindex) != MX_SUCCESS) {
    errno = EINVAL;
    return -1;
  }

  return hwloc_mx_board_get_device_cpuset(topology, nindex, set);
}

/** @} */


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* HWLOC_MYRIEXPRESS_H */
