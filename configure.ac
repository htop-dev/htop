#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# ----------------------------------------------------------------------
# Autoconf initialization.
# ----------------------------------------------------------------------

AC_PREREQ([2.69])
AC_INIT([htop], [3.0.5-dev], [htop@groups.io], [], [https://htop.dev/])

AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall std-options subdir-objects])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for platform.
# ----------------------------------------------------------------------

case "$target_os" in
linux*|gnu*)
   my_htop_platform=linux
   AC_DEFINE([HTOP_LINUX], [], [Building for Linux.])
   ;;
freebsd*|kfreebsd*)
   my_htop_platform=freebsd
   AC_DEFINE([HTOP_FREEBSD], [], [Building for FreeBSD.])
   ;;
openbsd*)
   my_htop_platform=openbsd
   AC_DEFINE([HTOP_OPENBSD], [], [Building for OpenBSD.])
   ;;
dragonfly*)
   my_htop_platform=dragonflybsd
   AC_DEFINE([HTOP_DRAGONFLYBSD], [], [Building for DragonFlyBSD.])
   ;;
darwin*)
   my_htop_platform=darwin
   AC_DEFINE([HTOP_DARWIN], [], [Building for Darwin.])
   ;;
solaris*)
   my_htop_platform=solaris
   AC_DEFINE([HTOP_SOLARIS], [], [Building for Solaris.])
   ;;
*)
   my_htop_platform=unsupported
   AC_DEFINE([HTOP_UNSUPPORTED], [], [Building for an unsupported platform.])
   ;;
esac

# Enable extensions, required by hwloc scripts
AC_USE_SYSTEM_EXTENSIONS

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for compiler.
# ----------------------------------------------------------------------

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_C99
AS_IF([test "x$ac_cv_prog_cc_c99" = xno], [AC_MSG_ERROR([htop is written in C99. A newer compiler is required.])])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for header files.
# ----------------------------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
   stdlib.h \
   string.h \
   strings.h \
   sys/param.h \
   sys/time.h \
   unistd.h], [], [AC_MSG_ERROR([Essential system C header file missing])])

AC_HEADER_MAJOR
dnl glibc 2.25 deprecates 'major' and 'minor' in <sys/types.h> and requires to
dnl include <sys/sysmacros.h>. However the logic in AC_HEADER_MAJOR has not yet
dnl been updated in Autoconf 2.69, so use a workaround:
m4_version_prereq([2.70], [],
   [if test "x$ac_cv_header_sys_mkdev_h" != xyes; then
      AC_CHECK_HEADER([sys/sysmacros.h], [AC_DEFINE([MAJOR_IN_SYSMACROS], [1],
         [Define to 1 if `major', `minor', and `makedev' are declared in <sys/sysmacros.h>.])])
   fi])

# optional system headers

AC_CHECK_HEADERS([execinfo.h])

if test "$my_htop_platform" = darwin; then
   AC_CHECK_HEADERS([mach/mach_time.h])
fi

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for typedefs and structures.
# ----------------------------------------------------------------------

AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for library functions.
# ----------------------------------------------------------------------

AC_FUNC_STAT

AC_SEARCH_LIBS([ceil], [m], [], [AC_MSG_ERROR([Can not find required function ceil()])])
AC_SEARCH_LIBS([dlopen], [dl dld], [], [AC_MSG_ERROR([Can not find required function dlopen()])])

if test "$my_htop_platform" = dragonflybsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([Can not find required function kvm_open()])])
fi

if test "$my_htop_platform" = freebsd; then
   AC_SEARCH_LIBS([devstat_checkversion], [devstat], [], [AC_MSG_ERROR([Can not find required function devstat_checkversion()])])
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([Can not find required function kvm_open()])])
fi

if test "$my_htop_platform" = openbsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([Can not find required function kvm_open()])])
fi

if test "$my_htop_platform" = solaris; then
   AC_SEARCH_LIBS([kstat_open], [kstat], [], [AC_MSG_ERROR([Can not find required function kstat_open()])])
   AC_SEARCH_LIBS([Pgrab_error], [proc], [], [AC_MSG_ERROR([Can not find required function Pgrab_error()])])
   AC_SEARCH_LIBS([free], [malloc], [], [AC_MSG_ERROR([Can not find required function free()])])
fi

# optional library functions

AC_CHECK_FUNCS([ \
   clock_gettime \
   faccessat \
   fstatat \
   host_get_clock_service \
   openat \
   readlinkat])

AC_SEARCH_LIBS([backtrace], [execinfo])
AC_SEARCH_LIBS([clock_gettime], [rt])

if test "$my_htop_platform" = darwin; then
   AC_CHECK_FUNCS([mach_timebase_info])
fi

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for libraries.
# ----------------------------------------------------------------------

AX_WITH_CURSES

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for corss-platform features and flags.
# ----------------------------------------------------------------------

AC_ARG_WITH([hwloc],
            [AS_HELP_STRING([--with-hwloc],
                            [Compile with hwloc support for CPU affinity, disables Linux affinity (off by default)])],
            [],
            [with_hwloc=no])
case "$with_hwloc" in
   no) ;;
   yes)
      AC_CHECK_HEADERS([hwloc.h], [], [AC_MSG_ERROR([Can not find required header hwloc.h])])
      AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [AC_MSG_ERROR([Can not find required library hwloc])])
      ;;
   *) AC_MSG_ERROR([bad value $with_hwloc for --with-hwloc option]) ;;
esac


AC_ARG_ENABLE([setuid],
              [AS_HELP_STRING([--enable-setuid],
                              [enable setuid support])],
              [],
              [enable_setuid=no])
case "$enable_setuid" in
   no) ;;
   yes) AC_DEFINE([HAVE_SETUID_ENABLED], [1], [Define if setuid support enabled.]) ;;
   *) AC_MSG_ERROR([bad value $enable_setuid for --enable-setuid option]) ;;
esac

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for Linux features and flags.
# ----------------------------------------------------------------------

AC_ARG_WITH([proc],
            [AS_HELP_STRING([--with-proc=DIR],
                            [Location of a Linux-compatible proc filesystem (default=/proc)])],
            [],
            [with_proc=/proc])
if test -z "$with_proc"; then
   AC_MSG_ERROR([bad empty value for --with-proc option])
fi
AC_DEFINE_UNQUOTED([PROCDIR], ["$with_proc"], [Path of proc filesystem.])


AC_ARG_ENABLE([openvz],
              [AS_HELP_STRING([--enable-openvz],
                              [enable OpenVZ support (off by default)])],
              [],
              [enable_openvz=no])
case "$enable_openvz" in
   no) ;;
   yes) AC_DEFINE([HAVE_OPENVZ], [1], [Define if OpenVZ support enabled.]) ;;
   *) AC_MSG_ERROR([bad value $enable_openvz for --enable-openvz option]) ;;
esac


AC_ARG_ENABLE([vserver],
              [AS_HELP_STRING([--enable-vserver],
                              [enable VServer support (off by default)])],
              [],
              [enable_vserver=no])
case "$enable_vserver" in
   no) ;;
   yes) AC_DEFINE([HAVE_VSERVER], [1], [Define if VServer support enabled.]) ;;
   *) AC_MSG_ERROR([bad value $enable_vserver for --enable-vserver option]) ;;
esac


AC_ARG_ENABLE([ancient_vserver],
              [AS_HELP_STRING([--enable-ancient-vserver],
                              [enable ancient VServer support (implies --enable-vserver) (off by default)])],
              [],
              [enable_ancient_vserver=no])
case "$enable_ancient_vserver" in
   no) ;;
   yes)
      AC_DEFINE([HAVE_VSERVER], [1], [Define if VServer support enabled.])
      AC_DEFINE([HAVE_ANCIENT_VSERVER], [1], [Define if ancient VServer support enabled.])
      ;;
   *) AC_MSG_ERROR([bad value $enable_ancient_vserver for --enable-ancient-vserver option]) ;;
esac


AC_ARG_ENABLE([linux_affinity],
              [AS_HELP_STRING([--enable-linux-affinity],
                              [enable Linux sched_setaffinity and sched_getaffinity for affinity support, mutual exclusive with hwloc (check by default)])],
              [],
              [enable_linux_affinity=check])
case "$enable_linux_affinity" in
   no) ;;
   check)
      if test "x$with_hwloc" = xyes; then
         # disable in favor of hwloc
         enable_linux_affinity=no
      else
         AC_MSG_CHECKING([for usable sched_setaffinity])
         AC_RUN_IFELSE([
            AC_LANG_PROGRAM([[
               #include <sched.h>
               #include <errno.h>
               static cpu_set_t cpuset;
            ]], [[
               CPU_ZERO(&cpuset);
               sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
               if (errno == ENOSYS) return 1;
            ]])],
            [
               enable_linux_affinity=yes
               AC_MSG_RESULT([yes])
            ], [
               enable_linux_affinity=no
               AC_MSG_RESULT([no])
            ], [AC_MSG_RESULT([yes (assumed while cross compiling)])])
      fi
      ;;
   yes)
      if test "x$with_hwloc" = xyes; then
         AC_MSG_ERROR([--with-hwloc and --enable-linux-affinity are mutual exclusive. Specify at most one of them.])
      fi
      ;;
   *) AC_MSG_ERROR([bad value $enable_linux_affinity for --enable-linux-affinity option]) ;;
esac
if test "$enable_linux_affinity" = yes; then
   AC_DEFINE([HAVE_LINUX_AFFINITY], [1], [Define if Linux sched_setaffinity and sched_getaffinity are to be used.])
fi


AC_ARG_WITH([delayacct],
            [AS_HELP_STRING([--with-delayacct],
                            [Compile with Linux delay accounting support (off by default)])],
            [],
            [with_delayacct=no])
case "$with_delayacct" in
   no) ;;
   yes)
      PKG_PROG_PKG_CONFIG()
      PKG_CHECK_MODULES(LIBNL3, libnl-3.0, [], [AC_MSG_ERROR([Can not find required library libnl-3])])
      PKG_CHECK_MODULES(LIBNL3GENL, libnl-genl-3.0, [], [AC_MSG_ERROR([Can not find required library libnl-genl-3.0])])
      CFLAGS="$CFLAGS $LIBNL3_CFLAGS $LIBNL3GENL_CFLAGS"
      LIBS="$LIBS $LIBNL3_LIBS $LIBNL3GENL_LIBS"
      AC_DEFINE([HAVE_DELAYACCT], [1], [Define if delay accounting support enabled.])
      ;;
   *) AC_MSG_ERROR([bad value $with_delayacct for --with-delayacct]) ;;
esac


AC_ARG_WITH([sensors],
            [AS_HELP_STRING([--with-sensors],
                            [Compile with libsensors support for reading temperature data. Only requires libsensors headers at compile time, at runtime libsensors is loaded via dlopen. (check by default)])],
            [],
            [with_sensors=check])
case "$with_sensors" in
   no) ;;
   check)
      with_sensors=yes
      AC_CHECK_HEADERS([sensors/sensors.h], [], [with_sensors=no])
      ;;
   yes) AC_CHECK_HEADERS([sensors/sensors.h], [], [AC_MSG_ERROR([Can not find require header sensors/sensors.h])]) ;;
   *) AC_MSG_ERROR([bad value $with_sensors for --with-sensors]) ;;
esac

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for compiler warnings.
# ----------------------------------------------------------------------

AM_CFLAGS="\
 -Wall\
 -Wcast-align\
 -Wcast-qual\
 -Wextra\
 -Wfloat-equal\
 -Wformat=2\
 -Wmissing-format-attribute\
 -Wmissing-noreturn\
 -Wmissing-prototypes\
 -Wpointer-arith\
 -Wshadow\
 -Wstrict-prototypes\
 -Wundef\
 -Wunused\
 -Wwrite-strings"

AX_CHECK_COMPILE_FLAG([-Wnull-dereference], [AM_CFLAGS="$AM_CFLAGS -Wnull-dereference"], , [-Werror])

AC_ARG_ENABLE([werror], [AS_HELP_STRING([--enable-werror], [Treat warnings as errors (default: warnings are not errors)])], [], [enable_werror=no])
case "$enable_werror" in
   no) ;;
   yes) AM_CFLAGS="$AM_CFLAGS -Werror" ;;
   *) AC_MSG_ERROR([bad value $enable_werror for --enable-werror]) ;;
esac

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug], [Enable asserts (default: asserts are disabled)])], [], [enable_debug=no])
case "$enable_debug" in
   yes) ;;
   no) AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG" ;;
   *) AC_MSG_ERROR([bad value $enable_debug for --enable-debug]) ;;
esac

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# We're done, let's go!
# ----------------------------------------------------------------------

AC_DEFINE_UNQUOTED([COPYRIGHT], ["(C) 2004-2019 Hisham Muhammad. (C) 2020 htop dev team."], [Copyright message.])

AM_CONDITIONAL([HTOP_DARWIN], [test "$my_htop_platform" = darwin])
AM_CONDITIONAL([HTOP_DRAGONFLYBSD], [test "$my_htop_platform" = dragonflybsd])
AM_CONDITIONAL([HTOP_FREEBSD], [test "$my_htop_platform" = freebsd])
AM_CONDITIONAL([HTOP_LINUX], [test "$my_htop_platform" = linux])
AM_CONDITIONAL([HTOP_OPENBSD], [test "$my_htop_platform" = openbsd])
AM_CONDITIONAL([HTOP_SOLARIS], [test "$my_htop_platform" = solaris])
AM_CONDITIONAL([HTOP_UNSUPPORTED], [test "$my_htop_platform" = unsupported])

AC_SUBST(my_htop_platform)
AC_CONFIG_FILES([Makefile htop.1])
AC_OUTPUT

if test "$my_htop_platform" = unsupported; then
   echo ""
   echo "****************************************************************"
   echo "WARNING! This platform is not currently supported by htop."
   echo ""
   echo "The code will build, but it will produce a dummy version of htop"
   echo "which shows no processes, using the files from the unsupported/"
   echo "directory. This is meant to be a skeleton, to be used as a"
   echo "starting point if you are porting htop to a new platform."
   echo "****************************************************************"
   echo ""
fi

AC_MSG_RESULT([
  ${PACKAGE_NAME} ${VERSION}

  platform:                  $my_htop_platform
  (Linux) proc directory:    $with_proc
  (Linux) openvz:            $enable_openvz
  (Linux) vserver:           $enable_vserver
  (Linux) ancient vserver:   $enable_ancient_vserver
  (Linux) affinity:          $enable_linux_affinity
  (Linux) delay accounting:  $with_delayacct
  (Linux) sensors:           $with_sensors
  unicode:                   $ax_cv_ncursesw
  hwloc:                     $with_hwloc
  setuid:                    $enable_setuid
  debug:                     $enable_debug
])
