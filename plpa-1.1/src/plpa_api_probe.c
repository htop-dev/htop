/* -*- c -*-
 *
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2008 Cisco, Inc.  All rights reserved.
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

/* Cache, just to make things a little more efficient */
static PLPA_NAME(api_type_t) cache = PLPA_NAME_CAPS(PROBE_UNSET);

/* The len value we find - not in public header, but used by the lib */
size_t PLPA_NAME(len) = 0;

int PLPA_NAME(api_probe_init)(void)
{
    PLPA_NAME(cpu_set_t) mask;
    int rc;
    size_t len;
    
    for (len = sizeof(mask); len != 0; len >>= 1) {
        rc = syscall(__NR_sched_getaffinity, 0, len, &mask);
        if (rc >= 0) {
            /* OK, kernel is happy with a get().  Validate w/ a set(). */
            /* Note that kernel may have told us the "proper" size */
            size_t tmp = (0 != rc) ? ((size_t) rc) : len;
            /* Pass mask=NULL, expect errno==EFAULT if tmp was OK
               as a length */
            rc = syscall(__NR_sched_setaffinity, 0, tmp, NULL);
            if ((rc < 0) && (errno == EFAULT)) {
                cache = PLPA_NAME_CAPS(PROBE_OK);
                PLPA_NAME(len) = tmp;
                rc = 0;
                break;
            }
        }
        if (errno == ENOSYS) {
            break; /* No point in looping */
        }
    }
    
    if (rc >= 0) {
        /* OK */
    } else if (errno == ENOSYS) {
        /* Kernel returns ENOSYS because there is no support for
           processor affinity */
        cache = PLPA_NAME_CAPS(PROBE_NOT_SUPPORTED);
    } else {
        /* Unknown! */
        cache = PLPA_NAME_CAPS(PROBE_UNKNOWN);
    }

    return 0;
}


int PLPA_NAME(api_probe)(PLPA_NAME(api_type_t) *api_type)
{
    int ret;

    /* Check to see that we're initialized */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == api_type) {
        return EINVAL;
    }

    /* All done */
    *api_type = cache;
    return 0;
}
