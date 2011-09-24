/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009-2010 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief Macros to help interaction between hwloc and OpenFabrics
 * verbs.
 *
 * Applications that use both hwloc and OpenFabrics verbs may want to
 * include this file so as to get topology information for OpenFabrics
 * hardware.
 *
 */

#ifndef HWLOC_OPENFABRICS_VERBS_H
#define HWLOC_OPENFABRICS_VERBS_H

#include <hwloc.h>
#include <hwloc/autogen/config.h>
#include <hwloc/linux.h>

#include <infiniband/verbs.h>


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup hwlocality_openfabrics OpenFabrics-Specific Functions
 * @{
 */

/** \brief Get the CPU set of logical processors that are physically
 * close to device \p ibdev.
 *
 * For the given OpenFabrics device \p ibdev, read the corresponding
 * kernel-provided cpumap file and return the corresponding CPU set.
 * This function is currently only implemented in a meaningful way for
 * Linux; other systems will simply get a full cpuset.
 */
static __hwloc_inline int
hwloc_ibv_get_device_cpuset(hwloc_topology_t topology __hwloc_attribute_unused,
			    struct ibv_device *ibdev, hwloc_cpuset_t set)
{
#ifdef HWLOC_LINUX_SYS
  /* If we're on Linux, use the verbs-provided sysfs mechanism to
     get the local cpus */
#define HWLOC_OPENFABRICS_VERBS_SYSFS_PATH_MAX 128
  char path[HWLOC_OPENFABRICS_VERBS_SYSFS_PATH_MAX];
  FILE *sysfile = NULL;

  sprintf(path, "/sys/class/infiniband/%s/device/local_cpus",
	  ibv_get_device_name(ibdev));
  sysfile = fopen(path, "r");
  if (!sysfile)
    return -1;

  hwloc_linux_parse_cpumap_file(sysfile, set);

  fclose(sysfile);
#else
  /* Non-Linux systems simply get a full cpuset */
  hwloc_bitmap_copy(set, hwloc_topology_get_complete_cpuset(topology));
#endif
  return 0;
}

/** @} */


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* HWLOC_OPENFABRICS_VERBS_H */
