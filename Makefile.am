AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = htop

dist_man_MANS = htop.1
EXTRA_DIST = $(dist_man_MANS) htop.desktop htop.png htop.svg \
install-sh autogen.sh missing
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png
appicondir = $(datadir)/icons/hicolor/scalable/apps
appicon_DATA = htop.svg

AM_CFLAGS += -pedantic -std=c99 -D_XOPEN_SOURCE_EXTENDED -DSYSCONFDIR=\"$(sysconfdir)\" -I"$(top_srcdir)/$(my_htop_platform)"
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
	CommandScreen.c \
	Compat.c \
	CPUMeter.c \
	CRT.c \
	DateMeter.c \
	DateTimeMeter.c \
	DiskIOMeter.c \
	DisplayOptionsPanel.c \
	EnvScreen.c \
	FunctionBar.c \
	Hashtable.c \
	Header.c \
	HostnameMeter.c \
	htop.c \
	IncSet.c \
	InfoScreen.c \
	ListItem.c \
	LoadAverageMeter.c \
	MainPanel.c \
	MemoryMeter.c \
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
	Settings.c \
	SignalsPanel.c \
	SwapMeter.c \
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
	CommandScreen.h \
	Compat.h \
	DateMeter.h \
	DateTimeMeter.h \
	DiskIOMeter.h \
	DisplayOptionsPanel.h \
	EnvScreen.h \
	FunctionBar.h \
	Hashtable.h \
	Header.h \
	HostnameMeter.h \
	IncSet.h \
	InfoScreen.h \
	ListItem.h \
	LoadAverageMeter.h \
	Macros.h \
	MainPanel.h \
	MemoryMeter.h \
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
	Settings.h \
	SignalsPanel.h \
	SwapMeter.h \
	TasksMeter.h \
	TraceScreen.h \
	UptimeMeter.h \
	UsersTable.h \
	Vector.h \
	XUtils.h

# Linux
# -----

linux_platform_headers = \
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

if HTOP_LINUX
AM_LDFLAGS += -rdynamic
myhtopplatsources = \
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
	zfs/ZfsArcStats.c \
	zfs/ZfsCompressedArcMeter.c

myhtopplatheaders = $(linux_platform_headers)
endif

# FreeBSD
# -------

freebsd_platform_headers = \
	freebsd/FreeBSDProcessList.h \
	freebsd/FreeBSDProcess.h \
	freebsd/Platform.h \
	freebsd/ProcessField.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/openzfs_sysctl.h

if HTOP_FREEBSD
myhtopplatsources = freebsd/Platform.c freebsd/FreeBSDProcessList.c \
freebsd/FreeBSDProcess.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c zfs/openzfs_sysctl.c

myhtopplatheaders = $(freebsd_platform_headers)
endif

# DragonFlyBSD
# ------------

dragonflybsd_platform_headers = \
	dragonflybsd/DragonFlyBSDProcessList.h \
	dragonflybsd/DragonFlyBSDProcess.h \
	dragonflybsd/Platform.h \
	dragonflybsd/ProcessField.h

if HTOP_DRAGONFLYBSD
myhtopplatsources = \
	dragonflybsd/Platform.c \
	dragonflybsd/DragonFlyBSDProcessList.c \
	dragonflybsd/DragonFlyBSDProcess.c

myhtopplatheaders = $(dragonflybsd_platform_headers)
endif

# OpenBSD
# -------

openbsd_platform_headers = \
	openbsd/OpenBSDProcessList.h \
	openbsd/OpenBSDProcess.h \
	openbsd/Platform.h \
	openbsd/ProcessField.h

if HTOP_OPENBSD
myhtopplatsources = openbsd/Platform.c openbsd/OpenBSDProcessList.c \
openbsd/OpenBSDProcess.c

myhtopplatheaders = $(openbsd_platform_headers)
endif

# Darwin
# ------

darwin_platform_headers = \
	darwin/DarwinProcess.h \
	darwin/DarwinProcessList.h \
	darwin/Platform.h \
	darwin/ProcessField.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/openzfs_sysctl.h

if HTOP_DARWIN
AM_LDFLAGS += -framework IOKit -framework CoreFoundation
myhtopplatsources = darwin/Platform.c darwin/DarwinProcess.c \
darwin/DarwinProcessList.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c zfs/openzfs_sysctl.c

myhtopplatheaders = $(darwin_platform_headers)
endif

# Solaris
# -------

solaris_platform_headers = \
	solaris/Platform.h \
	solaris/ProcessField.h \
	solaris/SolarisProcess.h \
	solaris/SolarisProcessList.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsCompressedArcMeter.h \
	zfs/ZfsArcStats.h

if HTOP_SOLARIS
myhtopplatsources = solaris/Platform.c \
solaris/SolarisProcess.c solaris/SolarisProcessList.c \
zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c zfs/ZfsArcStats.c

myhtopplatheaders = $(solaris_platform_headers)
endif

# Unsupported
# -----------

unsupported_platform_headers = \
	unsupported/Platform.h \
	unsupported/ProcessField.h \
	unsupported/UnsupportedProcess.h \
	unsupported/UnsupportedProcessList.h

if HTOP_UNSUPPORTED
myhtopplatsources = unsupported/Platform.c \
unsupported/UnsupportedProcess.c unsupported/UnsupportedProcessList.c

myhtopplatheaders = $(unsupported_platform_headers)
endif

# ----

htop_SOURCES  = $(myhtopheaders) $(myhtopplatheaders) $(myhtopsources) $(myhtopplatsources)
nodist_htop_SOURCES = config.h

target:
	echo $(htop_SOURCES)

profile:
	$(MAKE) all AM_CPPFLAGS="-pg -O2 -DNDEBUG"

debug:
	$(MAKE) all AM_CPPFLAGS="-ggdb -DDEBUG"

coverage:
	$(MAKE) all AM_CPPFLAGS="-fprofile-arcs -ftest-coverage -DDEBUG" AM_LDFLAGS="-lgcov"

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
