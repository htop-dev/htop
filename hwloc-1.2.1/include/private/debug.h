/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009, 2011 Université Bordeaux 1
 * Copyright © 2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/* The configuration file */

#ifndef HWLOC_DEBUG_H
#define HWLOC_DEBUG_H

#include <private/autogen/config.h>

#ifdef HWLOC_DEBUG
#include <stdarg.h>
#include <stdio.h>
#endif

static __hwloc_inline void hwloc_debug(const char *s __hwloc_attribute_unused, ...)
{
#ifdef HWLOC_DEBUG
    va_list ap;

    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
#endif
}

#ifdef HWLOC_DEBUG
#define hwloc_debug_bitmap(fmt, bitmap) do { \
  char *s= hwloc_bitmap_printf_value(bitmap); \
  fprintf(stderr, fmt, s); \
  free(s); \
} while (0)
#define hwloc_debug_1arg_bitmap(fmt, arg1, bitmap) do { \
  char *s= hwloc_bitmap_printf_value(bitmap); \
  fprintf(stderr, fmt, arg1, s); \
  free(s); \
} while (0)
#define hwloc_debug_2args_bitmap(fmt, arg1, arg2, bitmap) do { \
  char *s= hwloc_bitmap_printf_value(bitmap); \
  fprintf(stderr, fmt, arg1, arg2, s); \
  free(s); \
} while (0)
#else
#define hwloc_debug_bitmap(s, bitmap) do { } while(0)
#define hwloc_debug_1arg_bitmap(s, arg1, bitmap) do { } while(0)
#define hwloc_debug_2args_bitmap(s, arg1, arg2, bitmap) do { } while(0)
#endif

#endif /* HWLOC_DEBUG_H */
