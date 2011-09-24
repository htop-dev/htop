/* hwloc-1.2.1/include/hwloc/autogen/config.h.  Generated from config.h.in by configure.  */
/* -*- c -*-
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/* The configuration file */

#ifndef HWLOC_CONFIG_H
#define HWLOC_CONFIG_H

#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
# define __hwloc_restrict __restrict
#else
# if __STDC_VERSION__ >= 199901L
#  define __hwloc_restrict restrict
# else
#  define __hwloc_restrict
# endif
#endif

#define __hwloc_inline __inline__

/*
 * Note: this is public.  We can not assume anything from the compiler used
 * by the application and thus the HWLOC_HAVE_* macros below are not
 * fetched from the autoconf result here. We only automatically use a few
 * well-known easy cases.
 */

/* Maybe before gcc 2.95 too */
#if defined(HWLOC_HAVE_ATTRIBUTE_UNUSED) || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
# if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)) || HWLOC_HAVE_ATTRIBUTE_UNUSED
#  define __hwloc_attribute_unused __attribute__((__unused__))
# else
#  define __hwloc_attribute_unused
# endif
#else
# define __hwloc_attribute_unused
#endif

#if defined(HWLOC_HAVE_ATTRIBUTE_MALLOC) || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
# if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)) || HWLOC_HAVE_ATTRIBUTE_MALLOC
#  define __hwloc_attribute_malloc __attribute__((__malloc__))
# else
#  define __hwloc_attribute_malloc
# endif
#else
# define __hwloc_attribute_malloc
#endif

#if defined(HWLOC_HAVE_ATTRIBUTE_CONST) || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
# if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)) || HWLOC_HAVE_ATTRIBUTE_CONST
#  define __hwloc_attribute_const __attribute__((__const__))
# else
#  define __hwloc_attribute_const
# endif
#else
# define __hwloc_attribute_const
#endif

#if defined(HWLOC_HAVE_ATTRIBUTE_PURE) || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
# if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)) || HWLOC_HAVE_ATTRIBUTE_PURE
#  define __hwloc_attribute_pure __attribute__((__pure__))
# else
#  define __hwloc_attribute_pure
# endif
#else
# define __hwloc_attribute_pure
#endif

#if defined(HWLOC_HAVE_ATTRIBUTE_DEPRECATED) || (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))
# if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)) || HWLOC_HAVE_ATTRIBUTE_DEPRECATED
#  define __hwloc_attribute_deprecated __attribute__((__deprecated__))
# else
#  define __hwloc_attribute_deprecated
# endif
#else
# define __hwloc_attribute_deprecated
#endif

#ifdef HWLOC_C_HAVE_VISIBILITY
# if HWLOC_C_HAVE_VISIBILITY
#  define HWLOC_DECLSPEC __attribute__((__visibility__("default")))
# else
#  define HWLOC_DECLSPEC
# endif
#else
# define HWLOC_DECLSPEC
#endif

/* Defined to 1 on Linux */
#define HWLOC_LINUX_SYS 1

/* Defined to 1 if the CPU_SET macro works */
#define HWLOC_HAVE_CPU_SET 1

/* Defined to 1 if you have the `windows.h' header. */
/* #undef HWLOC_HAVE_WINDOWS_H */
#define hwloc_pid_t pid_t
#define hwloc_thread_t pthread_t

#ifdef HWLOC_HAVE_WINDOWS_H

#  include <windows.h>
typedef DWORDLONG hwloc_uint64_t;

#else /* HWLOC_HAVE_WINDOWS_H */

#  ifdef hwloc_thread_t
#    include <pthread.h>
#  endif /* hwloc_thread_t */

/* Defined to 1 if you have the <stdint.h> header file. */
#  define HWLOC_HAVE_STDINT_H 1

#  include <unistd.h>
#  ifdef HWLOC_HAVE_STDINT_H
#    include <stdint.h>
#  endif
typedef uint64_t hwloc_uint64_t;

#endif /* HWLOC_HAVE_WINDOWS_H */

/* Whether we need to re-define all the hwloc public symbols or not */
#define HWLOC_SYM_TRANSFORM 0

/* The hwloc symbol prefix */
#define HWLOC_SYM_PREFIX hwloc_

/* The hwloc symbol prefix in all caps */
#define HWLOC_SYM_PREFIX_CAPS HWLOC_

#endif /* HWLOC_CONFIG_H */
