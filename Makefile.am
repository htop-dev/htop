
ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = htop

dist_man_MANS = htop.1
EXTRA_DIST = $(dist_man_MANS) htop.desktop htop.png \
install-sh autogen.sh missing
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png

AM_CFLAGS += -pedantic -Wall $(wextra_flag) -std=c99 -D_XOPEN_SOURCE_EXTENDED -DSYSCONFDIR=\"$(sysconfdir)\" -I"$(top_srcdir)/$(my_htop_platform)"
AM_LDFLAGS =
AM_CPPFLAGS = -DNDEBUG

myhtopsources = AvailableMetersPanel.c CategoriesPanel.c CheckItem.c \
ClockMeter.c ColorsPanel.c ColumnsPanel.c CPUMeter.c CRT.c MainPanel.c \
DisplayOptionsPanel.c FunctionBar.c Hashtable.c Header.c htop.c ListItem.c \
LoadAverageMeter.c MemoryMeter.c Meter.c MetersPanel.c Object.c Panel.c \
BatteryMeter.c Process.c ProcessList.c RichString.c ScreenManager.c Settings.c \
SignalsPanel.c StringUtils.c SwapMeter.c TasksMeter.c UptimeMeter.c \
TraceScreen.c UsersTable.c Vector.c AvailableColumnsPanel.c AffinityPanel.c \
HostnameMeter.c OpenFilesScreen.c Affinity.c IncSet.c Action.c EnvScreen.c \
InfoScreen.c XAlloc.c

myhtopheaders = AvailableColumnsPanel.h AvailableMetersPanel.h \
CategoriesPanel.h CheckItem.h ClockMeter.h ColorsPanel.h ColumnsPanel.h \
CPUMeter.h CRT.h MainPanel.h DisplayOptionsPanel.h FunctionBar.h \
Hashtable.h Header.h ListItem.h LoadAverageMeter.h MemoryMeter.h \
BatteryMeter.h Meter.h MetersPanel.h Object.h Panel.h ProcessList.h RichString.h \
ScreenManager.h Settings.h SignalsPanel.h StringUtils.h SwapMeter.h \
TasksMeter.h UptimeMeter.h TraceScreen.h UsersTable.h Vector.h Process.h \
AffinityPanel.h HostnameMeter.h OpenFilesScreen.h Affinity.h IncSet.h Action.h \
EnvScreen.h InfoScreen.h XAlloc.h Macros.h

# Linux
# -----

linux_platform_headers = \
	linux/Platform.h \
	linux/IOPriorityPanel.h \
	linux/IOPriority.h \
	linux/LinuxProcess.h \
	linux/LinuxProcessList.h \
	linux/LinuxCRT.h \
	linux/Battery.h \
	linux/PressureStallMeter.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h

if HTOP_LINUX
AM_LDFLAGS += -rdynamic
myhtopplatsources = linux/Platform.c linux/IOPriorityPanel.c \
linux/LinuxProcess.c linux/LinuxProcessList.c linux/LinuxCRT.c linux/Battery.c \
linux/PressureStallMeter.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c

myhtopplatheaders = $(linux_platform_headers)
endif

# FreeBSD
# -------

freebsd_platform_headers = \
	freebsd/Platform.h \
	freebsd/FreeBSDProcessList.h \
	freebsd/FreeBSDProcess.h \
	freebsd/FreeBSDCRT.h \
	freebsd/Battery.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/openzfs_sysctl.h

if HTOP_FREEBSD
myhtopplatsources = freebsd/Platform.c freebsd/FreeBSDProcessList.c \
freebsd/FreeBSDProcess.c freebsd/FreeBSDCRT.c freebsd/Battery.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c zfs/openzfs_sysctl.c

myhtopplatheaders = $(freebsd_platform_headers)
endif

# DragonFlyBSD
# ------------

dragonflybsd_platform_headers = \
	dragonflybsd/Platform.h \
	dragonflybsd/DragonFlyBSDProcessList.h \
	dragonflybsd/DragonFlyBSDProcess.h \
	dragonflybsd/DragonFlyBSDCRT.h \
	dragonflybsd/Battery.h

if HTOP_DRAGONFLYBSD
AM_LDFLAGS += -lkvm -lkinfo -lexecinfo
myhtopplatsources = dragonflybsd/Platform.c dragonflybsd/DragonFlyBSDProcessList.c \
dragonflybsd/DragonFlyBSDProcess.c dragonflybsd/DragonFlyBSDCRT.c dragonflybsd/Battery.c

myhtopplatheaders = $(dragonflybsd_platform_headers)
endif

# OpenBSD
# -------

openbsd_platform_headers = \
	openbsd/Platform.h \
	openbsd/OpenBSDProcessList.h \
	openbsd/OpenBSDProcess.h \
	openbsd/OpenBSDCRT.h \
	openbsd/Battery.h

if HTOP_OPENBSD
myhtopplatsources = openbsd/Platform.c openbsd/OpenBSDProcessList.c \
openbsd/OpenBSDProcess.c openbsd/OpenBSDCRT.c openbsd/Battery.c

myhtopplatheaders = $(openbsd_platform_headers)
endif

# Darwin
# ------

darwin_platform_headers = \
	darwin/Platform.h \
	darwin/DarwinProcess.h \
	darwin/DarwinProcessList.h \
	darwin/DarwinCRT.h \
	darwin/Battery.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/openzfs_sysctl.h

if HTOP_DARWIN
AM_LDFLAGS += -framework IOKit -framework CoreFoundation
myhtopplatsources = darwin/Platform.c darwin/DarwinProcess.c \
darwin/DarwinProcessList.c darwin/DarwinCRT.c darwin/Battery.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c zfs/openzfs_sysctl.c

myhtopplatheaders = $(darwin_platform_headers)
endif

# Solaris
# -------

solaris_platform_headers = \
	solaris/Platform.h \
	solaris/SolarisProcess.h \
	solaris/SolarisProcessList.h \
	solaris/SolarisCRT.h \
	solaris/Battery.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h

if HTOP_SOLARIS
myhtopplatsources = solaris/Platform.c \
solaris/SolarisProcess.c solaris/SolarisProcessList.c \
solaris/SolarisCRT.c solaris/Battery.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c

myhtopplatheaders = $(solaris_platform_headers)
endif

# Unsupported
# -----------

unsupported_platform_headers = \
	unsupported/Platform.h \
	unsupported/UnsupportedProcess.h \
	unsupported/UnsupportedProcessList.h \
	unsupported/UnsupportedCRT.h \
	unsupported/Battery.h

if HTOP_UNSUPPORTED
myhtopplatsources = unsupported/Platform.c \
unsupported/UnsupportedProcess.c unsupported/UnsupportedProcessList.c \
unsupported/UnsupportedCRT.c unsupported/Battery.c

myhtopplatheaders = $(unsupported_platform_headers)
endif

# ----

htop_SOURCES  = $(myhtopheaders) $(myhtopplatheaders) $(myhtopsources) $(myhtopplatsources)
nodist_htop_SOURCES = config.h

target:
	echo $(htop_SOURCES)

profile:
	$(MAKE) all CFLAGS="-pg" AM_CPPFLAGS="-pg -O2 -DNDEBUG"

debug:
	$(MAKE) all CFLAGS="" AM_CPPFLAGS="-ggdb -DDEBUG"

coverage:
	$(MAKE) all CFLAGS="" AM_CPPFLAGS="-fprofile-arcs -ftest-coverage -DDEBUG" LDFLAGS="-lgcov"

cppcheck:
	cppcheck -q -v . --enable=all -DHAVE_CGROUP -DHAVE_OPENVZ -DHAVE_TASKSTATS

dist-hook: $(top_distdir)/configure
	@if grep 'pkg_m4_absent' '$(top_distdir)/configure'; then \
	   echo 'configure is generated without pkg.m4. Please supply pkg.m4 and run ./autogen.sh to rebuild the configure script.'>&2; \
	   (exit 1); \
	else :; \
	fi

.PHONY: lcov

lcov:
	mkdir -p lcov
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory lcov
