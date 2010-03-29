/* -*- c -*-
 *
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2008 Cisco, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * Some notes about the declarations and definitions in this file:
 *
 * This file is a mix of internal and public declarations.
 * Applications are warned against using the internal types; they are
 * subject to change with no warning.
 *
 * The PLPA_NAME() and PLPA_NAME_CAPS() macros are used for prefixing
 * the PLPA type names, enum names, and symbol names when embedding
 * PLPA.  When not embedding, the default prefix is "plpa_" (or
 * "PLPA_" when using PLPA_NAME_CAPS()).  Hence, if you see a
 * declaration like this:
 *
 * int PLPA_NAME(foo)(void);
 *
 * It's a function named plpa_foo() that returns an int and takes no
 * arguments when building PLPA as a standalone library.  It's a
 * function with a different prefix than "plpa_" when the
 * --enable-included-mode and --with-plpa-symbol-prefix options are
 * supplied to PLPA's configure script.
 *
 * Note that this header file differentiates between a
 * processor/socket/core ID and a processor/socket/core number.  The
 * "ID" is the integer that is used by Linux to identify that entity.
 * These integers may or may not be contiguous.  The "number" is a
 * contiguous set of integers starting with 0 and going to (count-1),
 * where (count) is the number of processors, sockets, and cores
 * (where the count of cores is dependent upon the socket).  Hence,
 * "number" is a human convenience, and "ID" is the actual Linux
 * identifier.
 */

#ifndef PLPA_H
#define PLPA_H

/* Absolutely must not include <sched.h> here or it will generate
   conflicts. */

/* For memset() */
#include <string.h>
/* For pid_t and size_t */
#include <sys/types.h>

/***************************************************************************
 * Internal types
 ***************************************************************************/

/* If we're building PLPA itself, <plpa_config.h> will have already
   been included.  But <plpa_config.h> is a private header file; it is
   not installed into $includedir.  Hence, applications including
   <plpa.h> will not have included <plpa_config.h> (this is by
   design).  So include just enough information here to allow us to
   continue. */
#ifndef PLPA_CONFIG_H
/* The PLPA symbol prefix */
#define PLPA_SYM_PREFIX plpa_

/* The PLPA symbol prefix in all caps */
#define PLPA_SYM_PREFIX_CAPS PLPA_
#endif

/* Preprocessors are fun -- the double inderection is unfortunately
   necessary. */
#define PLPA_MUNGE_NAME(a, b) PLPA_MUNGE_NAME2(a, b)
#define PLPA_MUNGE_NAME2(a, b) a ## b
#define PLPA_NAME(name) PLPA_MUNGE_NAME(PLPA_SYM_PREFIX, name)
#define PLPA_NAME_CAPS(name) PLPA_MUNGE_NAME(PLPA_SYM_PREFIX_CAPS, name)

/***************************************************************************
 * Public type
 ***************************************************************************/

/* Values that can be returned from plpa_api_probe() */
typedef enum {
    /* Sentinel value */
    PLPA_NAME_CAPS(PROBE_UNSET),
    /* sched_setaffinity syscall available */
    PLPA_NAME_CAPS(PROBE_OK),
    /* syscall unavailable/unimplemented */
    PLPA_NAME_CAPS(PROBE_NOT_SUPPORTED),
    /* we experienced some strange failure that the user should report */
    PLPA_NAME_CAPS(PROBE_UNKNOWN)
} PLPA_NAME(api_type_t);

/***************************************************************************
 * Internal types
 ***************************************************************************/

/* Internal PLPA bitmask type.  This type should not be used by
   external applications! */
typedef unsigned long int PLPA_NAME(bitmask_t);
#define PLPA_BITMASK_T_NUM_BITS (sizeof(PLPA_NAME(bitmask_t)) * 8)
#define PLPA_BITMASK_CPU_MAX 1024
#define PLPA_BITMASK_NUM_ELEMENTS (PLPA_BITMASK_CPU_MAX / PLPA_BITMASK_T_NUM_BITS)

/***************************************************************************
 * Public type
 ***************************************************************************/

/* Public type for the PLPA cpu set. */
typedef struct { PLPA_NAME(bitmask_t) bitmask[PLPA_BITMASK_NUM_ELEMENTS]; } PLPA_NAME(cpu_set_t);

/***************************************************************************
 * Internal macros
 ***************************************************************************/

/* Internal macro for identifying the byte in a bitmask array.  This
   macro should not be used by external applications! */
#define PLPA_CPU_BYTE(num) ((num) / PLPA_BITMASK_T_NUM_BITS)

/* Internal macro for identifying the bit in a bitmask array.  This
   macro should not be used by external applications! */
#define PLPA_CPU_BIT(num) ((num) % PLPA_BITMASK_T_NUM_BITS)

/***************************************************************************
 * Public macros
 ***************************************************************************/

/* Public macro to zero out a PLPA cpu set (analogous to the FD_ZERO()
   macro; see select(2)). */
#define PLPA_CPU_ZERO(cpuset) \
    memset((cpuset), 0, sizeof(PLPA_NAME(cpu_set_t)))

/* Public macro to set a bit in a PLPA cpu set (analogous to the
   FD_SET() macro; see select(2)). */
#define PLPA_CPU_SET(num, cpuset) \
    (cpuset)->bitmask[PLPA_CPU_BYTE(num)] |= ((PLPA_NAME(bitmask_t))1 << PLPA_CPU_BIT(num))

/* Public macro to clear a bit in a PLPA cpu set (analogous to the
   FD_CLR() macro; see select(2)). */
#define PLPA_CPU_CLR(num, cpuset) \
    (cpuset)->bitmask[PLPA_CPU_BYTE(num)] &= ~((PLPA_NAME(bitmask_t))1 << PLPA_CPU_BIT(num))

/* Public macro to test if a bit is set in a PLPA cpu set (analogous
   to the FD_ISSET() macro; see select(2)). */
#define PLPA_CPU_ISSET(num, cpuset) \
    (0 != (((cpuset)->bitmask[PLPA_CPU_BYTE(num)]) & ((PLPA_NAME(bitmask_t))1 << PLPA_CPU_BIT(num))))

/***************************************************************************
 * Public functions
 ***************************************************************************/

/* Setup PLPA internals.  This function is optional; it will be
   automatically invoked by all the other API functions if you do not
   invoke it explicitly.  Returns 0 upon success. */
int PLPA_NAME(init)(void);

/* Check what API is on this machine.  If api_type returns
   PLPA_PROBE_OK, then PLPA can function properly on this machine.
   Returns 0 upon success. */
int PLPA_NAME(api_probe)(PLPA_NAME(api_type_t) *api_type);

/* Set processor affinity.  Use the PLPA_CPU_* macros to set the
   cpuset value.  The same rules and restrictions about pid apply as
   they do for the sched_setaffinity(2) system call.  Bits set in the
   CPU mask correspond to Linux processor IDs.  Returns 0 upon
   success. */
int PLPA_NAME(sched_setaffinity)(pid_t pid, size_t cpusetsize,
                                 const PLPA_NAME(cpu_set_t) *cpuset);

/* Get processor affinity.  Use the PLPA_CPU_* macros to analyze the
   returned value of cpuset.  The same rules and restrictions about
   pid apply as they do for the sched_getaffinity(2) system call.
   Bits set in the CPU mask corresopnd to Linux processor IDs.
   Returns 0 upon success. */
int PLPA_NAME(sched_getaffinity)(pid_t pid, size_t cpusetsize,
                                 PLPA_NAME(cpu_set_t) *cpuset);

/* Return whether topology information is available (i.e.,
   plpa_map_to_*, plpa_max_*).  The topology functions will be
   available if supported == 1 and the function returns 0. */
int PLPA_NAME(have_topology_information)(int *supported);

/* Map (socket_id,core_id) tuple to virtual processor ID.  processor_id is
   then suitable for use with the PLPA_CPU_* macros, probably leading
   to a call to plpa_sched_setaffinity().  Returns 0 upon success. */
int PLPA_NAME(map_to_processor_id)(int socket_id, int core_id,
                                   int *processor_id);

/* Map processor_id to (socket_id,core_id) tuple.  The processor_id input is
   usually obtained from the return from the plpa_sched_getaffinity()
   call, using PLPA_CPU_ISSET to find individual bits in the map that
   were set/unset.  plpa_map_to_socket_core() can map the bit indexes
   to a socket/core tuple.  Returns 0 upon success. */
int PLPA_NAME(map_to_socket_core)(int processor_id, 
                                  int *socket_id, int *core_id);

/* This function is deprecated and will disappear in a future release.
   It is exactly equivalent to calling
   plpa_get_processor_data(PLPA_COUNT_ALL, num_processors,
   max_processor_id). */
int PLPA_NAME(get_processor_info)(int *num_processors, int *max_processor_id);

/* Typedefs for specifying which processors / sockets / cores to count
   in get_processor_data() and get_processor_id() */
typedef enum {
    /* Only count online processors */
    PLPA_NAME_CAPS(COUNT_ONLINE),
    /* Only count offline processors */
    PLPA_NAME_CAPS(COUNT_OFFLINE),
    /* Count all processors (online and offline) */
    PLPA_NAME_CAPS(COUNT_ALL)
} PLPA_NAME(count_specification_t);

/* Returns both the number of processors in a system and the maximum
   Linux virtual processor ID (because it may be higher than the
   number of processors if there are "holes" in the available Linux
   virtual processor IDs).  The count_spec argument specifies whether
   to count all processors, only online processors, or only offline
   processors.  Returns 0 upon success.  */
int PLPA_NAME(get_processor_data)(PLPA_NAME(count_specification_t) count_spec,
                                  int *num_processors, int *max_processor_id);

/* Returns the Linux processor ID for the Nth processor.  For example,
   if the Linux processor IDs have "holes", use this function to say
   "give me the Linux processor ID of the 4th processor."  count_spec
   specifies whether to count online, offline, or all processors when
   looking for the processor_num'th processor.  Returns 0 upon
   success. */
int PLPA_NAME(get_processor_id)(int processor_num, 
                                PLPA_NAME(count_specification_t) count_spec,
                                int *processor_id);

/* Check to see if a given Linux processor ID exists / is online.
   Returns 0 on success. */
int PLPA_NAME(get_processor_flags)(int processor_id, int *exists, int *online);

/* Returns both the number of sockets in the system and the maximum
   socket ID number (in case there are "holes" in the list of available
   socket IDs).  Returns 0 upon sucess. */
int PLPA_NAME(get_socket_info)(int *num_sockets, int *max_socket_id);

/* Returns the Linux socket ID for the Nth socket.  For example, if
   the socket IDs have "holes", use this function to say "give me the
   Linux socket ID of the 2nd socket."  Linux does not specify the
   socket/core tuple information for offline processors, so a
   plpa_count_specification_t parameter is not used here.  Returns 0
   upon success. */
int PLPA_NAME(get_socket_id)(int socket_num, int *socket_id);

/* Return both the number of cores and the max code ID for a given
   socket (in case there are "holes" in the list of available core
   IDs).  Returns 0 upon success. */
int PLPA_NAME(get_core_info)(int socket_id, int *num_cores, int *max_core_id);

/* Given a specific socket, returns the Linux core ID for the Nth
   core.  For example, if the core IDs have "holes", use this function
   to say "give me the Linux core ID of the 4th core on socket ID 7."
   Linux does not specify the socket/core tuple information for
   offline processors, so a plpa_count_specification_t parameter is
   not used here.  Returns 0 upon success.  Returns 0 upon success. */
int PLPA_NAME(get_core_id)(int socket_id, int core_num, int *core_id);

/* Check to see if a given Linux (socket_id,core_id) tuple exists / is
   online.  Returns 0 on success. */
int PLPA_NAME(get_core_flags)(int socket_id, int core_id,
                              int *exists, int *online);

/* Typedefs for specifying the cache behavior via
   plpa_set_cache_behavior() */
typedef enum {
    /* Use the cache (default behavior); fills the cache right now if
       it's not already full */
    PLPA_NAME_CAPS(CACHE_USE),
    /* Never use the cache; always look up the information in
       the kernel */
    PLPA_NAME_CAPS(CACHE_IGNORE),
    /* Refresh the cache right now */
    PLPA_NAME_CAPS(CACHE_REFRESH)
} PLPA_NAME(cache_behavior_t);

/* Set PLPA's cache behavior.  Returns 0 upon success. */
int PLPA_NAME(set_cache_behavior)(PLPA_NAME(cache_behavior_t));

/* Shut down PLPA.  This function releases resources used by the PLPA.
   It should be the last PLPA function invoked, or can be used to
   forcibly cause PLPA to dump its topology cache and re-analyze the
   underlying system the next time another PLPA function is called.
   Specifically: it is safe to call plpa_init() (or any other PLPA
   function) again after plpa_finalized().  Returns 0 upon success. */
int PLPA_NAME(finalize)(void);

#endif /* PLPA_H */

