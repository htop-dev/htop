os_sources = files(
	'Battery.c',
	'Platform.c',
	'SolarisCRT.c',
	'SolarisProcess.c',
	'SolarisProcessList.c',
)

os_inc = include_directories('.')

add_project_arguments(['-D__EXTENSIONS__', '-D_POSIX_PTHREAD_SEMANTICS'], language: 'c')

proc  = cc.find_library('proc')
kstat = cc.find_library('kstat')

libs += [proc, kstat]
