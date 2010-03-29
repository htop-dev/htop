/*
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 */

#include "plpa_config.h"
#include "plpa.h"
#include "plpa_internal.h"

#include <errno.h>
#include <pthread.h>

/* Global variables */
int PLPA_NAME(initialized) = 0;

/* Local variables */
static int refcount = 0;
static pthread_mutex_t mutex;


/* Central clearing point for all parts of PLPA that need to be
   initialized.  It is erroneous to call this function by more than
   one thread simultaneously. */
int PLPA_NAME(init)(void)
{
    int ret;

    /* If we're already initialized, simply increase the refcount */
    if (PLPA_NAME(initialized)) {
        pthread_mutex_lock(&mutex);
        ++refcount;
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    /* Otherwise, initialize all the sybsystems */
    if (0 != (ret = pthread_mutex_init(&mutex, NULL)) ||
        0 != (ret = PLPA_NAME(api_probe_init)()) ||
        0 != (ret = PLPA_NAME(set_cache_behavior)(PLPA_NAME_CAPS(CACHE_USE)))) {
        return ret;
    }

    PLPA_NAME(initialized) = 1;
    refcount = 1;
    return 0;
}


/* Central clearing point for all parts of PLPA that need to be
   shutdown. */
int PLPA_NAME(finalize)(void)
{
    int val;

    /* If we're not initialized, return an error */
    if (!PLPA_NAME(initialized)) {
        return ENOENT;
    }

    /* Decrement and check the refcount.  If it's nonzero, then simply
       return success. */
    pthread_mutex_lock(&mutex);
    val = --refcount;
    pthread_mutex_unlock(&mutex);
    if (0 != val) {
        return 0;
    }

    /* Ok, we're the last one.  Cleanup. */
    PLPA_NAME(set_cache_behavior)(PLPA_NAME_CAPS(CACHE_IGNORE));
    pthread_mutex_destroy(&mutex);
    PLPA_NAME(initialized) = 0;
    return 0;
}
