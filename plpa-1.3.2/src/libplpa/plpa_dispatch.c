/* -*- c -*-
 *
 * Copyright (c) 2004-2006 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2008 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "plpa_config.h"
#include "plpa.h"
#include "plpa_internal.h"

#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * Call the kernel's setaffinity, massaging the user's input
 * parameters as necessary
 */
int PLPA_NAME(sched_setaffinity)(pid_t pid, size_t cpusetsize,
                                 const PLPA_NAME(cpu_set_t) *cpuset)
{
    int ret;
    size_t i;
    PLPA_NAME(cpu_set_t) tmp;
    PLPA_NAME(api_type_t) api;

    /* Check to see that we're initialized */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == cpuset) {
        return EINVAL;
    }

    /* Probe the API type */
    if (0 != (ret = PLPA_NAME(api_probe)(&api))) {
        return ret;
    }
    switch (api) {
    case PLPA_NAME_CAPS(PROBE_OK):
        /* This shouldn't happen, but check anyway */
        if (cpusetsize > sizeof(*cpuset)) {
            return EINVAL;
        }

        /* If the user-supplied bitmask is smaller than what the
           kernel wants, zero out a temporary buffer of the size that
           the kernel wants and copy the user-supplied bitmask to the
           lower part of the temporary buffer.  This could be done
           more efficiently, but we're looking for clarity/simplicity
           of code here -- this is not intended to be
           performance-critical. */
        if (cpusetsize < PLPA_NAME(len)) {
            memset(&tmp, 0, sizeof(tmp));
            for (i = 0; i < cpusetsize * 8; ++i) {
                if (PLPA_CPU_ISSET(i, cpuset)) {
                    PLPA_CPU_SET(i, &tmp);
                }
            }
        }

        /* If the user-supplied bitmask is larger than what the kernel
           will accept, scan it and see if there are any set bits in
           the part larger than what the kernel will accept.  If so,
           return EINVAL.  Otherwise, copy the part that the kernel
           will accept into a temporary and use that.  Again,
           efficinency is not the issue of this code -- clarity is. */
        else if (cpusetsize > PLPA_NAME(len)) {
            for (i = PLPA_NAME(len) * 8; i < cpusetsize * 8; ++i) {
                if (PLPA_CPU_ISSET(i, cpuset)) {
                    return EINVAL;
                }
            }
            /* No upper-level bits are set, so now copy over the bits
               that the kernel will look at */
            memset(&tmp, 0, sizeof(tmp));
            for (i = 0; i < PLPA_NAME(len) * 8; ++i) {
                if (PLPA_CPU_ISSET(i, cpuset)) {
                    PLPA_CPU_SET(i, &tmp);
                }
            }
        }

        /* Otherwise, the user supplied a buffer that is exactly the
           right size.  Just for clarity of code, copy the user's
           buffer into the temporary and use that. */
        else {
            memcpy(&tmp, cpuset, cpusetsize);
        }

        /* Now do the syscall */
        ret = syscall(__NR_sched_setaffinity, pid, PLPA_NAME(len), &tmp);

        /* Return 0 upon success.  According to
           http://www.open-mpi.org/community/lists/plpa-users/2006/02/0016.php,
           all the kernel implementations return >= 0 upon success. */
        return (ret >= 0) ? 0 : ret;
        break;

    case PLPA_NAME_CAPS(PROBE_NOT_SUPPORTED):
        /* Process affinity not supported here */
        return ENOSYS;
        break;

    default:
        /* Something went wrong */
        /* JMS: would be good to have something other than EINVAL here
           -- suggestions? */
        return EINVAL;
        break;
    }
}


/**
 * Call the kernel's getaffinity, massaging the user's input
 * parameters as necessary
 */
int PLPA_NAME(sched_getaffinity)(pid_t pid, size_t cpusetsize,
                                PLPA_NAME(cpu_set_t) *cpuset)
{
    int ret;
    PLPA_NAME(api_type_t) api;

    /* Check to see that we're initialized */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == cpuset) {
        return EINVAL;
    }
    /* Probe the API type */
    if (0 != (ret = PLPA_NAME(api_probe)(&api))) {
        return ret;
    }
    switch (api) {
    case PLPA_NAME_CAPS(PROBE_OK):
        /* This shouldn't happen, but check anyway */
        if (PLPA_NAME(len) > sizeof(*cpuset)) {
            return EINVAL;
        }

        /* If the user supplied a buffer that is too small, then don't
           even bother */
        if (cpusetsize < PLPA_NAME(len)) {
            return EINVAL;
        }

        /* Now we know that the user's buffer is >= the size required
           by the kernel.  If it's >, then zero it out so that the
           bits at the top are cleared (since they won't be set by the
           kernel) */
        if (cpusetsize > PLPA_NAME(len)) {
            memset(cpuset, 0, cpusetsize);
        }

        /* Now do the syscall */
        ret = syscall(__NR_sched_getaffinity, pid, PLPA_NAME(len), cpuset);

        /* Return 0 upon success.  According to
           http://www.open-mpi.org/community/lists/plpa-users/2006/02/0016.php,
           all the kernel implementations return >= 0 upon success. */
        return (ret >= 0) ? 0 : ret;
        break;

    case PLPA_NAME_CAPS(PROBE_NOT_SUPPORTED):
        /* Process affinity not supported here */
        return ENOSYS;
        break;

    default:
        /* Something went wrong */
        return EINVAL;
        break;
    }
}

