/* -*- c -*-
 *
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
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

#ifndef PLPA_INTERNAL_H
#define PLPA_INTERNAL_H

#include <plpa.h>

/* Have we initialized yet? */
extern int PLPA_NAME(initialized);

/* Cached size of the affinity buffers that the kernel expects */
extern size_t PLPA_NAME(len);

/* Setup API type */
int PLPA_NAME(api_probe_init)(void);

#endif /* PLPA_INTERNAL_H */

