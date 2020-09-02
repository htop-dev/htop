project(
	'htop',
	'c',
	version: '3.2.0',
	meson_version: '>=0.53.0',
	default_options: [
		'c_std=c99',
		'warning_level=2',
	],
)

add_project_arguments(
	'-DSYSCONFDIR="@0@"'.format(get_option('sysconfdir')),
	language: 'c'
)

if get_option('debug')
	add_project_arguments('-DDEBUG', language: 'c')
endif

cc   = meson.get_compiler('c')
os   = host_machine.system()
conf = configuration_data()

conf.set_quoted('COPYRIGHT', '(C) 2004-2018 Hisham Muhammad')
conf.set_quoted('VERSION', meson.project_version())

zfs_sources = []
zfs_inc = ''

math   = cc.find_library('m', required: false)

# Here goes the curses detection, ugh...
nc_libs    = []
nc_headers = []

if get_option('unicode')
	nc_libs += ['ncursesw']
	nc_headers += [
		'ncursesw/ncurses.h',
		'ncursesw.h'
	]
	# Special header incantations for wide char support
	add_project_arguments([
		'-D_XOPEN_SOURCE_EXTENDED',
		'-DNCURSES_WIDECHAR'],
		language: 'c')
	conf.set10('HAVE_LIBNCURSESW', true)
endif

nc_libs  += [
	'ncurses',
	'curses'
]
nc_headers += [
	'ncurses/ncurses.h',
	'ncurses/curses.h',
	'ncurses.h',
	'curses.h',
]

nc_found = false
foreach lib : nc_libs
	curses = dependency(lib, required: false)
	if not curses.found()
		curses = cc.find_library(lib, required: false)
	endif
	if curses.found()
		nc_found = true
		break
	endif
endforeach
if not nc_found
	error('could not find ncurses library')
endif

header_found = false
foreach h : nc_headers
	if cc.has_header(h, dependencies: curses)
		conf.set10('HAVE_@0@'.format(h.underscorify().to_upper()), true)
		header_found = true
		break
	endif
endforeach
if not header_found
	error('could not find ncurses header')
endif

libs = [math, curses]

if os == 'darwin'
	subdir('darwin')
	subdir('zfs')
	zfs_sources += 'zfs/openzfs_sysctl.c'
elif os == 'dragonfly'
	subdir('dragonflybsd')
elif os == 'freebsd'
	subdir('freebsd')
	subdir('zfs')
	zfs_sources += 'zfs/openzfs_sysctl.c'
elif os == 'linux'
	subdir('linux')
	subdir('zfs')
elif os == 'openbsd'
	subdir('openbsd')
elif os == 'sunos'
	subdir('solaris')
	subdir('zfs')
else
	warning('''
This platform is not currently supported by htop.
The code will build, but it will produce a dummy version of htop
which shows no processes, using the files from the unsupported/
directory. This is meant to be a skeleton, to be used as a
starting point if you are porting htop to a new platform.''')
	subdir('unsupported')
endif

conf.set(
	'HAVE_EXECINFO_H',
	cc.has_header('execinfo.h')
)
conf.set10(
	'HAVE_SETUID_ENABLED',
	get_option('setuid')
)

if not cc.has_header_symbol('sys/types.h', 'major')
	if cc.has_header_symbol('sys/mkdev.h', 'major')
		conf.set('MAJOR_IN_MKDEV', true)
	elif cc.has_header_symbol('sys/sysmacros.h', 'major')
		conf.set('MAJOR_IN_SYSMACROS', true)
	endif
endif

if get_option('linux-affinity') and get_option('hwloc').enabled()
	error('cannot use linux-affinity and hwloc together')
endif
hwloc = dependency('hwloc', required: get_option('hwloc'))
if hwloc.found()
	conf.set('HAVE_LIBHWLOC', true)
endif
libs += hwloc

sources = files(
	'Action.c',
	'Affinity.c',
	'AffinityPanel.c',
	'AvailableColumnsPanel.c',
	'AvailableMetersPanel.c',
	'BatteryMeter.c',
	'CPUMeter.c',
	'CRT.c',
	'CategoriesPanel.c',
	'CheckItem.c',
	'ClockMeter.c',
	'ColorsPanel.c',
	'ColumnsPanel.c',
	'DisplayOptionsPanel.c',
	'EnvScreen.c',
	'FunctionBar.c',
	'Hashtable.c',
	'Header.c',
	'HostnameMeter.c',
	'IncSet.c',
	'InfoScreen.c',
	'ListItem.c',
	'LoadAverageMeter.c',
	'MainPanel.c',
	'MemoryMeter.c',
	'Meter.c',
	'MetersPanel.c',
	'Object.c',
	'OpenFilesScreen.c',
	'Panel.c',
	'Process.c',
	'ProcessList.c',
	'RichString.c',
	'ScreenManager.c',
	'Settings.c',
	'SignalsPanel.c',
	'StringUtils.c',
	'SwapMeter.c',
	'TasksMeter.c',
	'TraceScreen.c',
	'UptimeMeter.c',
	'UsersTable.c',
	'Vector.c',
	'XAlloc.c',
	'htop.c',
)

configure_file(
	output: 'config.h',
	configuration: conf
)

summary({
	'platform': 'os',
	'proc directory': get_option('procdir'),
	'openvz': get_option('openvz'),
	'cgroups': get_option('cgroups'),
	'vserver': get_option('vserver'),
	'taskstats': get_option('taskstats'),
	'unicode': get_option('unicode'),
	'linux affinity': get_option('linux-affinity'),
	'hwloc': get_option('hwloc').enabled(),
	'setuid': get_option('setuid'),
})

executable(
	'htop',
	os_sources, sources, zfs_sources,
	include_directories: [os_inc, zfs_inc],
	dependencies: libs,
	install: true,
)

install_data(
	'htop.desktop',
	install_dir: get_option('datadir') / 'applications',
)
install_data(
	'htop.png',
	install_dir: get_option('datadir') / 'pixmaps',
)
