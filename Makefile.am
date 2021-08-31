if !HTOP_PCP
bin_PROGRAMS = htop
myhtopplatprogram = htop.c
else
bin_PROGRAMS = pcp-htop
myhtopplatprogram = pcp-htop.c
endif

dist_man_MANS = htop.1
EXTRA_DIST = \
	$(dist_man_MANS) \
	autogen.sh \
	htop.desktop \
	htop.png \
	htop.svg \
	build-aux/compile \
	build-aux/depcomp \
	build-aux/install-sh \
	build-aux/missing
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png
appicondir = $(datadir)/icons/hicolor/scalable/apps
appicon_DATA = htop.svg

AM_CFLAGS += -pedantic -std=c99 -D_XOPEN_SOURCE_EXTENDED -DSYSCONFDIR="\"$(sysconfdir)\"" -I"$(top_srcdir)/$(my_htop_platform)"
AM_LDFLAGS =

myhtopsources = \
	Action.c \
	Affinity.c \
	AffinityPanel.c \
	AvailableColumnsPanel.c \
	AvailableMetersPanel.c \
	BatteryMeter.c \
	CategoriesPanel.c \
	ClockMeter.c \
	ColorsPanel.c \
	ColumnsPanel.c \
	CommandLine.c \
	CommandScreen.c \
	Compat.c \
	CPUMeter.c \
	CRT.c \
	DateMeter.c \
	DateTimeMeter.c \
	DiskIOMeter.c \
	DisplayOptionsPanel.c \
	DynamicColumn.c \
	DynamicMeter.c \
	EnvScreen.c \
	FunctionBar.c \
	Hashtable.c \
	Header.c \
	HeaderOptionsPanel.c \
	HostnameMeter.c \
	IncSet.c \
	InfoScreen.c \
	ListItem.c \
	LoadAverageMeter.c \
	MainPanel.c \
	MemoryMeter.c \
	MemorySwapMeter.c \
	Meter.c \
	MetersPanel.c \
	NetworkIOMeter.c \
	Object.c \
	OpenFilesScreen.c \
	OptionItem.c \
	Panel.c \
	Process.c \
	ProcessList.c \
	ProcessLocksScreen.c \
	RichString.c \
	ScreenManager.c \
	ScreensPanel.c \
	Settings.c \
	SignalsPanel.c \
	SwapMeter.c \
	SysArchMeter.c \
	TasksMeter.c \
	TraceScreen.c \
	UptimeMeter.c \
	UsersTable.c \
	Vector.c \
	XUtils.c

myhtopheaders = \
	Action.h \
	Affinity.h \
	AffinityPanel.h \
	AvailableColumnsPanel.h \
	AvailableMetersPanel.h \
	BatteryMeter.h \
	CPUMeter.h \
	CRT.h \
	CategoriesPanel.h \
	ClockMeter.h \
	ColorsPanel.h \
	ColumnsPanel.h \
	CommandLine.h \
	CommandScreen.h \
	Compat.h \
	DateMeter.h \
	DateTimeMeter.h \
	DiskIOMeter.h \
	DisplayOptionsPanel.h \
	DynamicColumn.h \
	DynamicMeter.h \
	EnvScreen.h \
	FunctionBar.h \
	Hashtable.h \
	Header.h \
	HeaderLayout.h \
	HeaderOptionsPanel.h \
	HostnameMeter.h \
	IncSet.h \
	InfoScreen.h \
	ListItem.h \
	LoadAverageMeter.h \
	Macros.h \
	MainPanel.h \
	MemoryMeter.h \
	MemorySwapMeter.h \
	Meter.h \
	MetersPanel.h \
	NetworkIOMeter.h \
	Object.h \
	OpenFilesScreen.h \
	OptionItem.h \
	Panel.h \
	Process.h \
	ProcessList.h \
	ProcessLocksScreen.h \
	ProvideCurses.h \
	RichString.h \
	ScreenManager.h \
	ScreensPanel.h \
	Settings.h \
	SignalsPanel.h \
	SwapMeter.h \
	SysArchMeter.h \
	TasksMeter.h \
	TraceScreen.h \
	UptimeMeter.h \
	UsersTable.h \
	Vector.h \
	XUtils.h

# Linux
# -----

linux_platform_headers = \
	generic/gettime.h \
	generic/hostname.h \
	generic/uname.h \
	linux/CGroupUtils.h \
	linux/HugePageMeter.h \
	linux/IOPriority.h \
	linux/IOPriorityPanel.h \
	linux/LibSensors.h \
	linux/LinuxProcess.h \
	linux/LinuxProcessList.h \
	linux/Platform.h \
	linux/PressureStallMeter.h \
	linux/ProcessField.h \
	linux/SELinuxMeter.h \
	linux/SystemdMeter.h \
	linux/ZramMeter.h \
	linux/ZramStats.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h

linux_platform_sources = \
	generic/gettime.c \
	generic/hostname.c \
	generic/uname.c \
	linux/CGroupUtils.c \
	linux/HugePageMeter.c \
	linux/IOPriorityPanel.c \
	linux/LibSensors.c \
	linux/LinuxProcess.c \
	linux/LinuxProcessList.c \
	linux/Platform.c \
	linux/PressureStallMeter.c \
	linux/SELinuxMeter.c \
	linux/SystemdMeter.c \
	linux/ZramMeter.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c

if HTOP_LINUX
AM_LDFLAGS += -rdynamic
myhtopplatheaders = $(linux_platform_headers)
myhtopplatsources = $(linux_platform_sources)
endif

# FreeBSD
# -------

freebsd_platform_headers = \
	freebsd/FreeBSDProcessList.h \
	freebsd/FreeBSDProcess.h \
	freebsd/Platform.h \
	freebsd/ProcessField.h \
	generic/gettime.h \
	generic/hostname.h \
	generic/openzfs_sysctl.h \
	generic/uname.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h

freebsd_platform_sources = \
	freebsd/Platform.c \
	freebsd/FreeBSDProcessList.c \
	freebsd/FreeBSDProcess.c \
	generic/gettime.c \
	generic/hostname.c \
	generic/openzfs_sysctl.c \
	generic/uname.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c

if HTOP_FREEBSD
myhtopplatheaders = $(freebsd_platform_headers)
myhtopplatsources = $(freebsd_platform_sources)
endif

# DragonFlyBSD
# ------------

dragonflybsd_platform_headers = \
	dragonflybsd/DragonFlyBSDProcessList.h \
	dragonflybsd/DragonFlyBSDProcess.h \
	dragonflybsd/Platform.h \
	dragonflybsd/ProcessField.h \
	generic/gettime.h \
	generic/hostname.h \
	generic/uname.h

dragonflybsd_platform_sources = \
	dragonflybsd/DragonFlyBSDProcessList.c \
	dragonflybsd/DragonFlyBSDProcess.c \
	dragonflybsd/Platform.c \
	generic/gettime.c \
	generic/hostname.c \
	generic/uname.c

if HTOP_DRAGONFLYBSD
myhtopplatheaders = $(dragonflybsd_platform_headers)
myhtopplatsources = $(dragonflybsd_platform_sources)
endif

# NetBSD
# -------

netbsd_platform_headers = \
	generic/gettime.h \
	generic/hostname.h \
	generic/uname.h \
	netbsd/Platform.h \
	netbsd/ProcessField.h \
	netbsd/NetBSDProcess.h \
	netbsd/NetBSDProcessList.h

netbsd_platform_sources = \
	generic/gettime.c \
	generic/hostname.c \
	generic/uname.c \
	netbsd/Platform.c \
	netbsd/NetBSDProcess.c \
	netbsd/NetBSDProcessList.c

if HTOP_NETBSD
myhtopplatheaders = $(netbsd_platform_headers)
myhtopplatsources = $(netbsd_platform_sources)
endif

# OpenBSD
# -------

openbsd_platform_headers = \
	generic/gettime.h \
	generic/hostname.h \
	generic/uname.h \
	openbsd/OpenBSDProcessList.h \
	openbsd/OpenBSDProcess.h \
	openbsd/Platform.h \
	openbsd/ProcessField.h

openbsd_platform_sources = \
	generic/gettime.c \
	generic/hostname.c \
	generic/uname.c \
	openbsd/OpenBSDProcessList.c \
	openbsd/OpenBSDProcess.c \
	openbsd/Platform.c

if HTOP_OPENBSD
myhtopplatheaders = $(openbsd_platform_headers)
myhtopplatsources = $(openbsd_platform_sources)
endif

# Darwin
# ------

darwin_platform_headers = \
	darwin/DarwinProcess.h \
	darwin/DarwinProcessList.h \
	darwin/Platform.h \
	darwin/PlatformHelpers.h \
	darwin/ProcessField.h \
	generic/gettime.h \
	generic/hostname.h \
	generic/openzfs_sysctl.h \
	generic/uname.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h

darwin_platform_sources = \
	darwin/Platform.c \
	darwin/PlatformHelpers.c \
	darwin/DarwinProcess.c \
	darwin/DarwinProcessList.c \
	generic/gettime.c \
	generic/hostname.c \
	generic/openzfs_sysctl.c \
	generic/uname.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c

if HTOP_DARWIN
AM_LDFLAGS += -framework IOKit -framework CoreFoundation
myhtopplatheaders = $(darwin_platform_headers)
myhtopplatsources = $(darwin_platform_sources)
endif

# Solaris
# -------

solaris_platform_headers = \
	generic/gettime.h \
	generic/hostname.h \
	generic/uname.h \
	solaris/ProcessField.h \
	solaris/Platform.h \
	solaris/SolarisProcess.h \
	solaris/SolarisProcessList.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h

solaris_platform_sources = \
	generic/gettime.c \
	generic/hostname.c \
	generic/uname.c \
	solaris/Platform.c \
	solaris/SolarisProcess.c \
	solaris/SolarisProcessList.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c

if HTOP_SOLARIS
myhtopplatheaders = $(solaris_platform_headers)
myhtopplatsources = $(solaris_platform_sources)
endif

# Performance Co-Pilot (PCP)
# --------------------------

pcp_platform_headers = \
	linux/PressureStallMeter.h \
	linux/ZramMeter.h \
	linux/ZramStats.h \
	pcp/PCPDynamicColumn.h \
	pcp/PCPDynamicMeter.h \
	pcp/PCPMetric.h \
	pcp/PCPProcess.h \
	pcp/PCPProcessList.h \
	pcp/Platform.h \
	pcp/ProcessField.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h

pcp_platform_sources = \
	linux/PressureStallMeter.c \
	linux/ZramMeter.c \
	pcp/PCPDynamicColumn.c \
	pcp/PCPDynamicMeter.c \
	pcp/PCPMetric.c \
	pcp/PCPProcess.c \
	pcp/PCPProcessList.c \
	pcp/Platform.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c

if HTOP_PCP
myhtopplatheaders = $(pcp_platform_headers)
myhtopplatsources = $(pcp_platform_sources)
pcp_htop_SOURCES  = $(myhtopplatprogram) $(myhtopheaders) $(myhtopplatheaders) $(myhtopsources) $(myhtopplatsources)
endif

# Unsupported
# -----------

unsupported_platform_headers = \
	generic/gettime.h \
	unsupported/Platform.h \
	unsupported/ProcessField.h \
	unsupported/UnsupportedProcess.h \
	unsupported/UnsupportedProcessList.h

unsupported_platform_sources = \
	generic/gettime.c \
	unsupported/Platform.c \
	unsupported/UnsupportedProcess.c \
	unsupported/UnsupportedProcessList.c

if HTOP_UNSUPPORTED
myhtopplatsources = $(unsupported_platform_sources)
myhtopplatheaders = $(unsupported_platform_headers)
endif

# ----

htop_SOURCES = $(myhtopplatprogram) $(myhtopheaders) $(myhtopplatheaders) $(myhtopsources) $(myhtopplatsources)
nodist_htop_SOURCES = config.h

target:
	echo $(htop_SOURCES)

profile:
	$(MAKE) all AM_CPPFLAGS="-pg -O2 -DNDEBUG"

debug:
	$(MAKE) all AM_CPPFLAGS="-ggdb3 -Og" CFLAGS="`printf ' %s ' "$(CFLAGS)"|sed -E 's#[[:space:]]-O[^[:space:]]+[[:space:]]# #g'` -ggdb3 -Og"

coverage:
	$(MAKE) all AM_CPPFLAGS="-fprofile-arcs -ftest-coverage" AM_LDFLAGS="-lgcov"

cppcheck:
	cppcheck -q -v . --enable=all -DHAVE_OPENVZ

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
