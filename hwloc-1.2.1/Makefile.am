# Copyright © 2009      INRIA.  All rights reserved.
# Copyright © 2009      Université Bordeaux 1
# Copyright © 2009-2010 Cisco Systems, Inc.  All rights reserved.
# See COPYING in top-level directory.

# Note that the -I directory must *exactly* match what was specified
# via AC_CONFIG_MACRO_DIR in configure.ac.
ACLOCAL_AMFLAGS = -I ./config

SUBDIRS = src include

# Do not let automake automatically add the non-standalone dirs to the
# distribution tarball if we're building in embedded mode.
DIST_SUBDIRS = $(SUBDIRS)

#
# "make distcheck" requires that tarballs are able to be able to "make
# dist", so we have to include config/distscript.csh.
#
EXTRA_DIST = \
        README VERSION COPYING AUTHORS \
        config/hwloc_get_version.sh \
        config/distscript.csh

#
# Build the top-level README file
#
