# Copyright © 2009-2010 INRIA.  All rights reserved.
# Copyright © 2009-2010 Université Bordeaux 1
# Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
# See COPYING in top-level directory.

# Only install the headers if we're in standalone mode (meaning:
# *don't* install the headers if we're in embedded mode).

if HWLOC_BUILD_STANDALONE
include_HEADERS = hwloc.h
include_hwlocdir = $(includedir)/hwloc
include_hwloc_HEADERS = \
        hwloc/bitmap.h \
        hwloc/cpuset.h \
        hwloc/helper.h \
        hwloc/myriexpress.h \
        hwloc/openfabrics-verbs.h \
        hwloc/cuda.h \
        hwloc/cudart.h \
        hwloc/rename.h
include_hwloc_autogendir = $(includedir)/hwloc/autogen
nodist_include_hwloc_autogen_HEADERS = hwloc/autogen/config.h 

noinst_HEADERS = \
        private/private.h \
        private/debug.h \
        private/misc.h \
        private/cpuid.h

if HWLOC_HAVE_LINUX
include_hwloc_HEADERS += \
        hwloc/linux.h \
        hwloc/linux-libnuma.h
endif HWLOC_HAVE_LINUX

if HWLOC_HAVE_SCHED_SETAFFINITY
include_hwloc_HEADERS += hwloc/glibc-sched.h
endif HWLOC_HAVE_SCHED_SETAFFINITY

endif HWLOC_BUILD_STANDALONE
