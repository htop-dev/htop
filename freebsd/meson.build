os_sources = files(
	'Battery.c',
	'FreeBSDCRT.c',
	'FreeBSDProcess.c',
	'FreeBSDProcessList.c',
	'Platform.c',
)

os_inc = include_directories('.')

kvm = cc.find_library('kvm')
libs += kvm
