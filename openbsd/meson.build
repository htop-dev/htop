os_sources = files(
	'Battery.c',
	'OpenBSDCRT.c',
	'OpenBSDProcess.c',
	'OpenBSDProcessList.c',
	'Platform.c',
)

os_inc = include_directories('.')

kvm = cc.find_library('kvm')
libs += kvm
