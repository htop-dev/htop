os_sources = files(
	'Battery.c',
	'IOPriorityPanel.c',
	'LinuxCRT.c',
	'LinuxProcess.c',
	'LinuxProcessList.c',
	'Platform.c',
	'PressureStallMeter.c',
)

os_inc = include_directories('.')

vserver = get_option('vserver')
if vserver == 'yes'
	conf.set('HAVE_VSERVER', true)
elif vserver == 'ancient'
	conf.set('HAVE_VSERVER', true)
	conf.set('HAVE_ANCIENT_VSERVER', true)
endif
conf.set10('HAVE_LINUX_AFFINITY', get_option('linux-affinity'))
conf.set_quoted('PROCDIR', get_option('procdir'))
conf.set('HAVE_TASKSTATS', get_option('taskstats'))
conf.set('HAVE_CGROUP', get_option('cgroups'))
conf.set('HAVE_OPENVZ', get_option('openvz'))

delayacct = dependency('libnl-3.0', required: get_option('delayacct'))
if delayacct.found()
	conf.set('HAVE_DELAYACCT', true)
endif
libs += delayacct

add_project_arguments('-D_DEFAULT_SOURCE', language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-DHTOP_LINUX', language: 'c')
