
bin_PROGRAMS = htop
dist_man_MANS = htop.1
EXTRA_DIST = $(dist_man_MANS) htop.desktop htop.png scripts/MakeHeader.py \
install-sh autogen.sh missing
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png

AM_CFLAGS = -pedantic -Wall -std=c99
AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\"

htop_SOURCES = AvailableMetersPanel.c CategoriesPanel.c ClockMeter.c \
CPUMeter.c CRT.c DebugMemory.c DisplayOptionsPanel.c FunctionBar.c \
Hashtable.c Header.c htop.c Panel.c ListItem.c LoadAverageMeter.c \
MemoryMeter.c Meter.c MetersPanel.c Object.c Process.c \
ProcessList.c RichString.c ScreenManager.c Settings.c SignalItem.c \
SignalsPanel.c String.c SwapMeter.c TasksMeter.c Vector.c \
UptimeMeter.c UsersTable.c AvailableMetersPanel.h CategoriesPanel.h \
ClockMeter.h config.h CPUMeter.h CRT.h debug.h DebugMemory.h \
DisplayOptionsPanel.h FunctionBar.h Hashtable.h Header.h htop.h Panel.h \
ListItem.h LoadAverageMeter.h MemoryMeter.h Meter.h \
MetersPanel.h Object.h Process.h ProcessList.h RichString.h ScreenManager.h \
Settings.h SignalItem.h SignalsPanel.h String.h SwapMeter.h TasksMeter.h \
Vector.h UptimeMeter.h UsersTable.h CheckItem.c CheckItem.h \
ColorsPanel.c ColorsPanel.h TraceScreen.c TraceScreen.h \
AvailableColumnsPanel.c AvailableColumnsPanel.h ColumnsPanel.c \
ColumnsPanel.h

profile:
	$(MAKE) all CFLAGS="-pg -O2"

debug:
	$(MAKE) all CFLAGS="-g -DDEBUG"

debuglite:
	$(MAKE) all CFLAGS="-g -DDEBUGLITE"
