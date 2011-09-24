/*
 * Copyright © 2010 INRIA.  All rights reserved.
 * Copyright © 2010 Université Bordeaux 1
 * Copyright © 2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/** \file
 * \brief Macros to help interaction between hwloc and the CUDA Driver API.
 *
 * Applications that use both hwloc and the CUDA Driver API may want to
 * include this file so as to get topology information for CUDA devices.
 *
 */

#ifndef HWLOC_CUDA_H
#define HWLOC_CUDA_H

#include <hwloc.h>
#include <hwloc/autogen/config.h>
#include <hwloc/linux.h>

#include <cuda.h>


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup hwlocality_cuda CUDA Driver API Specific Functions
 * @{
 */

/** \brief Get the CPU set of logical processors that are physically
 * close to device \p cudevice.
 *
 * For the given CUDA Driver API device \p cudevice, read the corresponding
 * kernel-provided cpumap file and return the corresponding CPU set.
 * This function is currently only implemented in a meaningful way for
 * Linux; other systems will simply get a full cpuset.
 */
static __hwloc_inline int
hwloc_cuda_get_device_cpuset(hwloc_topology_t topology __hwloc_attribute_unused,
			     CUdevice cudevice, hwloc_cpuset_t set)
{
#ifdef HWLOC_LINUX_SYS
  /* If we're on Linux, use the sysfs mechanism to get the local cpus */
#define HWLOC_CUDA_DEVICE_SYSFS_PATH_MAX 128
  CUresult cres;
  int deviceid;
  int busid;
  char path[HWLOC_CUDA_DEVICE_SYSFS_PATH_MAX];
  FILE *sysfile = NULL;

  cres = cuDeviceGetAttribute(&busid, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, cudevice);
  if (cres != CUDA_SUCCESS) {
    errno = ENOSYS;
    return -1;
  }
  cres = cuDeviceGetAttribute(&deviceid, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, cudevice);
  if (cres != CUDA_SUCCESS) {
    errno = ENOSYS;
    return -1;
  }

  sprintf(path, "/sys/bus/pci/devices/0000:%02x:%02x.0/local_cpus", busid, deviceid);
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


#endif /* HWLOC_CUDA_H */
