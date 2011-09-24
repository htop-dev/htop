# Copyright © 2009-2010 INRIA.  All rights reserved.
# Copyright © 2009-2010 Université Bordeaux 1
# Copyright © 2009-2010 Cisco Systems, Inc.  All rights reserved.
# See COPYING in top-level directory.

AM_CFLAGS = $(HWLOC_CFLAGS)
AM_CPPFLAGS = $(HWLOC_CPPFLAGS)
AM_LDFLAGS = $(HWLOC_LDFLAGS)

EXTRA_DIST = dolib.c

# If we're in standalone mode, build the installable library.
# Otherwise, build the embedded library.

if HWLOC_BUILD_STANDALONE
lib_LTLIBRARIES = libhwloc.la
else
noinst_LTLIBRARIES = libhwloc_embedded.la
endif

# Sources and ldflags

sources = \
        topology.c \
        traversal.c \
        distances.c \
        topology-synthetic.c \
        bind.c \
        cpuset.c \
        misc.c
ldflags =

# Conditionally add to the sources and ldflags

if HWLOC_HAVE_XML
sources += topology-xml.c
endif HWLOC_HAVE_XML

if HWLOC_HAVE_SOLARIS
sources += topology-solaris.c
endif HWLOC_HAVE_SOLARIS

if HWLOC_HAVE_LINUX
sources += topology-linux.c
endif HWLOC_HAVE_LINUX

if HWLOC_HAVE_AIX
sources += topology-aix.c
ldflags += -lpthread
endif HWLOC_HAVE_AIX

if HWLOC_HAVE_OSF
sources += topology-osf.c
ldflags += -lnuma -lpthread
endif HWLOC_HAVE_OSF

if HWLOC_HAVE_HPUX
sources += topology-hpux.c
ldflags += -lpthread
endif HWLOC_HAVE_HPUX

if HWLOC_HAVE_WINDOWS
sources += topology-windows.c
endif HWLOC_HAVE_WINDOWS

if HWLOC_HAVE_DARWIN
sources += topology-darwin.c
endif HWLOC_HAVE_DARWIN

if HWLOC_HAVE_FREEBSD
sources += topology-freebsd.c
endif HWLOC_HAVE_FREEBSD

if HWLOC_HAVE_GCC
ldflags += -no-undefined
endif HWLOC_HAVE_GCC

if HWLOC_HAVE_WINDOWS
LC_MESSAGES=C
export LC_MESSAGES
ldflags += -Xlinker --output-def -Xlinker .libs/libhwloc.def

if HWLOC_HAVE_MS_LIB
.libs/libhwloc.lib: libhwloc.la dolib
	./dolib "$(HWLOC_MS_LIB)" X86 .libs/libhwloc.def libhwloc-$(HWLOC_SOVERSION) .libs/libhwloc.lib
all-local: .libs/libhwloc.lib
endif HWLOC_HAVE_MS_LIB

install-exec-hook:
	$(INSTALL) .libs/libhwloc.def $(DESTDIR)$(libdir)
if HWLOC_HAVE_MS_LIB
	$(INSTALL) .libs/libhwloc.lib $(DESTDIR)$(libdir)
	$(INSTALL) .libs/libhwloc.exp $(DESTDIR)$(libdir)
endif HWLOC_HAVE_MS_LIB
endif HWLOC_HAVE_WINDOWS

if HWLOC_HAVE_CPUID
sources += topology-x86.c
endif HWLOC_HAVE_CPUID

# Installable library

libhwloc_la_SOURCES = $(sources)
libhwloc_la_LDFLAGS = $(ldflags) -version-number $(libhwloc_so_version) $(HWLOC_XML_LIBS) $(HWLOC_LINUX_LIBNUMA_LIBS)

# Embedded library (note the lack of a .so version number -- that
# intentionally only appears in the installable library)

libhwloc_embedded_la_SOURCES = $(sources)
libhwloc_embedded_la_LDFLAGS = $(ldflags) $(HWLOC_XML_LIBS) $(HWLOC_LINUX_LIBNUMA_LIBS)

# XML data (only install if we're building in standalone mode)

if HWLOC_BUILD_STANDALONE
xml_DATA = $(srcdir)/hwloc.dtd
xmldir = $(pkgdatadir)
EXTRA_DIST += hwloc.dtd
endif
