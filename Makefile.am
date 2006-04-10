
bin_PROGRAMS = htop
dist_man_MANS = htop.1
EXTRA_DIST = $(dist_man_MANS) htop.desktop htop.png scripts/MakeHeader.py
applicationsdir = $(datadir)/applications
applications_DATA = htop.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = htop.png

AM_CFLAGS = -pedantic -Wall -std=c99
AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\"

htop_SOURCES = AvailableMetersListBox.c CategoriesListBox.c ClockMeter.c \
CPUMeter.c CRT.c DebugMemory.c DisplayOptionsListBox.c FunctionBar.c \
Hashtable.c Header.c htop.c ListBox.c ListItem.c LoadAverageMeter.c \
MemoryMeter.c Meter.c MetersListBox.c Object.c Process.c \
ProcessList.c RichString.c ScreenManager.c Settings.c SignalItem.c \
SignalsListBox.c String.c SwapMeter.c TasksMeter.c TypedVector.c \
UptimeMeter.c UsersTable.c AvailableMetersListBox.h CategoriesListBox.h \
ClockMeter.h config.h CPUMeter.h CRT.h debug.h DebugMemory.h \
DisplayOptionsListBox.h FunctionBar.h Hashtable.h Header.h htop.h ListBox.h \
ListItem.h LoadAverageMeter.h MemoryMeter.h Meter.h \
MetersListBox.h Object.h Process.h ProcessList.h RichString.h ScreenManager.h \
Settings.h SignalItem.h SignalsListBox.h String.h SwapMeter.h TasksMeter.h \
TypedVector.h UptimeMeter.h UsersTable.h CheckItem.c CheckItem.h \
ColorsListBox.c ColorsListBox.h TraceScreen.c TraceScreen.h \
AvailableColumnsListBox.c AvailableColumnsListBox.h ColumnsListBox.c \
ColumnsListBox.h

debug:
	$(MAKE) all CFLAGS="-g -DDEBUG"
