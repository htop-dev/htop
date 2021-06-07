#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# ----------------------------------------------------------------------
# Autoconf initialization.
# ----------------------------------------------------------------------

AC_PREREQ([2.69])
AC_INIT([htop], [3.0.6-dev], [htop@groups.io], [], [https://htop.dev/])

AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_AUX_DIR([build-aux])
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
# Checks for static build.
# ----------------------------------------------------------------------

AC_ARG_ENABLE([static],
              [AS_HELP_STRING([--enable-static],
                              [build a static htop binary @<:@default=no@:>@])],
              [],
              [enable_static=no])
case "$enable_static" in
   no)
      ;;
   yes)
      AC_DEFINE([BUILD_STATIC], [1], [Define if building static binary.])
      CFLAGS="$CFLAGS -static"
      LDFLAGS="$LDFLAGS -static"
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_static' for --enable-static option])
      ;;
esac

# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Checks for a PCP-based htop build.  (https://pcp.io)
# ----------------------------------------------------------------------

AC_ARG_ENABLE([pcp],
              [AS_HELP_STRING([--enable-pcp],
                              [build a pcp-htop binary @<:@default=no@:>@])],
              [],
              [enable_pcp=no])
case "$enable_pcp" in
   no)
      ;;
   yes)
      AC_CHECK_HEADERS([pcp/pmapi.h], [my_htop_platform=pcp],
		       [AC_MSG_ERROR([can not find PCP header file])])
      AC_SEARCH_LIBS([pmNewContext], [pcp], [], [AC_MSG_ERROR([can not find PCP library])])
      AC_DEFINE([HTOP_PCP], [1], [Define if building pcp-htop binary.])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_pcp' for --enable-pcp option])
      ;;
esac

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for generic header files.
# ----------------------------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
    stdlib.h \
    string.h \
    strings.h \
    sys/param.h \
    sys/time.h \
    sys/utsname.h \
    unistd.h
   ], [], [AC_MSG_ERROR([can not find required generic header files])])

AC_HEADER_MAJOR
dnl glibc 2.25 deprecates 'major' and 'minor' in <sys/types.h> and requires to
dnl include <sys/sysmacros.h>. However the logic in AC_HEADER_MAJOR has not yet
dnl been updated in Autoconf 2.69, so use a workaround:
m4_version_prereq([2.70], [],
[if test "x$ac_cv_header_sys_mkdev_h" != xyes; then
   AC_CHECK_HEADER([sys/sysmacros.h], [AC_DEFINE([MAJOR_IN_SYSMACROS], [1],
      [Define to 1 if `major', `minor', and `makedev' are declared in <sys/sysmacros.h>.])])
fi])

# Optional Section

AC_CHECK_HEADERS([execinfo.h])

if test "$my_htop_platform" = darwin; then
   AC_CHECK_HEADERS([mach/mach_time.h])
fi

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for typedefs, structures, and compiler characteristics.
# ----------------------------------------------------------------------

AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for generic library functions.
# ----------------------------------------------------------------------

AC_CHECK_LIB([m], [ceil], [], [AC_MSG_ERROR([can not find required function ceil()])])

if test "$my_htop_platform" = dragonflybsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([can not find required function kvm_open()])])
fi

if test "$my_htop_platform" = freebsd; then
   if test "$enable_static" = yes; then
      AC_SEARCH_LIBS([elf_version], [elf], [], [AC_MSG_ERROR([can not find required function elf_version()])])
   fi
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([can not find required function kvm_open()])])
   AC_SEARCH_LIBS([devstat_checkversion], [devstat], [], [AC_MSG_ERROR([can not find required function devstat_checkversion()])])
fi

if test "$my_htop_platform" = linux; then
   if test "$enable_static" != yes; then
      AC_SEARCH_LIBS([dlopen], [dl dld], [], [AC_MSG_ERROR([can not find required function dlopen()])])
   fi
fi

if test "$my_htop_platform" = openbsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([can not find required function kvm_open()])])
fi

if test "$my_htop_platform" = solaris; then
   AC_SEARCH_LIBS([kstat_open], [kstat], [], [AC_MSG_ERROR([can not find required function kstat_open()])])
   AC_SEARCH_LIBS([Pgrab_error], [proc], [], [AC_MSG_ERROR([can not find required function Pgrab_error()])])
   AC_SEARCH_LIBS([free], [malloc], [], [AC_MSG_ERROR([can not find required function free()])])
fi

# Optional Section

AC_SEARCH_LIBS([clock_gettime], [rt])

AC_CHECK_FUNCS([ \
    clock_gettime \
    faccessat \
    fstatat \
    host_get_clock_service \
    memfd_create\
    openat \
    readlinkat \
   ])

# Add -lexecinfo if needed
AC_SEARCH_LIBS([backtrace], [execinfo])

if test "$my_htop_platform" = darwin; then
   AC_CHECK_FUNCS([mach_timebase_info])
fi

if test "$my_htop_platform" = linux && test "x$enable_static" = xyes; then
   AC_CHECK_LIB([systemd], [sd_bus_open_system])
fi

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for cross-platform features and flags.
# ----------------------------------------------------------------------

# HTOP_CHECK_SCRIPT(LIBNAME, FUNCTION, DEFINE, CONFIG_SCRIPT, ELSE_PART)
m4_define([HTOP_CHECK_SCRIPT],
[
   if test ! -z "m4_toupper($HTOP_[$1]_CONFIG_SCRIPT)"; then
      # to be used to set the path to ncurses*-config when cross-compiling
      htop_config_script_libs=$(m4_toupper($HTOP_[$1]_CONFIG_SCRIPT) --libs 2> /dev/null)
      htop_config_script_cflags=$(m4_toupper($HTOP_[$1]_CONFIG_SCRIPT) --cflags 2> /dev/null)
   else
      htop_config_script_libs=$([$4] --libs 2> /dev/null)
      htop_config_script_cflags=$([$4] --cflags 2> /dev/null)
   fi
   htop_script_success=no
   htop_save_CFLAGS="$CFLAGS"
   if test ! "x$htop_config_script_libs" = x; then
      CFLAGS="$htop_config_script_cflags $CFLAGS"
      AC_CHECK_LIB([$1], [$2], [
         AC_DEFINE([$3], 1, [The library is present.])
         LIBS="$htop_config_script_libs $LIBS "
         htop_script_success=yes
      ], [
         CFLAGS="$htop_save_CFLAGS"
      ], [
         $htop_config_script_libs
      ])
   fi
   if test "x$htop_script_success" = xno; then
      [$5]
   fi
])

# HTOP_CHECK_LIB(LIBNAME, FUNCTION, DEFINE, ELSE_PART)
m4_define([HTOP_CHECK_LIB],
[
   AC_CHECK_LIB([$1], [$2], [
      AC_DEFINE([$3], [1], [The library is present.])
      LIBS="-l[$1] $LIBS "
   ], [$4])
])

AC_ARG_ENABLE([unicode],
              [AS_HELP_STRING([--enable-unicode],
                              [enable Unicode support @<:@default=yes@:>@])],
              [],
              [enable_unicode=yes])
if test "x$enable_unicode" = xyes; then
   HTOP_CHECK_SCRIPT([ncursesw6], [waddwstr], [HAVE_LIBNCURSESW], "ncursesw6-config",
    HTOP_CHECK_SCRIPT([ncursesw], [waddwstr], [HAVE_LIBNCURSESW], "ncursesw6-config",
     HTOP_CHECK_SCRIPT([ncursesw], [wadd_wch], [HAVE_LIBNCURSESW], "ncursesw5-config",
      HTOP_CHECK_SCRIPT([ncurses], [wadd_wch], [HAVE_LIBNCURSESW], "ncurses5-config",
       HTOP_CHECK_LIB([ncursesw6], [addnwstr], [HAVE_LIBNCURSESW],
        HTOP_CHECK_LIB([ncursesw], [addnwstr], [HAVE_LIBNCURSESW],
         HTOP_CHECK_LIB([ncurses], [addnwstr], [HAVE_LIBNCURSESW],
          AC_MSG_ERROR([can not find required library libncursesw; you may want to use --disable-unicode])
   )))))))

   AC_CHECK_HEADERS([ncursesw/curses.h], [],
      [AC_CHECK_HEADERS([ncurses/ncurses.h], [],
         [AC_CHECK_HEADERS([ncurses/curses.h], [],
            [AC_CHECK_HEADERS([ncurses.h], [],
               [AC_MSG_ERROR([can not find required ncurses header file])])])])])

   # check if additional linker flags are needed for keypad(3)
   # (at this point we already link against a working ncurses library with wide character support)
   AC_SEARCH_LIBS([keypad], [tinfow tinfo])
else
   HTOP_CHECK_SCRIPT([ncurses6], [wnoutrefresh], [HAVE_LIBNCURSES], [ncurses6-config],
    HTOP_CHECK_SCRIPT([ncurses], [wnoutrefresh], [HAVE_LIBNCURSES], [ncurses5-config],
     HTOP_CHECK_LIB([ncurses6],  [doupdate], [HAVE_LIBNCURSES],
      HTOP_CHECK_LIB([ncurses],  [doupdate], [HAVE_LIBNCURSES],
       AC_MSG_ERROR([can not find required library libncurses])
   ))))

   AC_CHECK_HEADERS([curses.h], [],
      [AC_CHECK_HEADERS([ncurses/curses.h], [],
         [AC_CHECK_HEADERS([ncurses/ncurses.h], [],
            [AC_CHECK_HEADERS([ncurses.h] ,[],
               [AC_MSG_ERROR([can not find required ncurses header file])])])])])

   # check if additional linker flags are needed for keypad(3)
   # (at this point we already link against a working ncurses library)
   AC_SEARCH_LIBS([keypad], [tinfo])
fi
if test "$enable_static" = yes; then
   AC_SEARCH_LIBS([Gpm_GetEvent], [gpm])
fi
if test "$my_htop_platform" = "solaris"; then
   # On OmniOS /usr/include/sys/regset.h redefines ERR to 13 - \r, breaking the Enter key.
   # Since ncruses macros use the ERR macro, we can not use another name.
   AC_DEFINE([ERR], [(-1)], [Predefine ncurses macro.])
fi


AC_ARG_ENABLE([hwloc],
              [AS_HELP_STRING([--enable-hwloc],
              [enable hwloc support for CPU affinity; disables Linux affinity; requires libhwloc @<:@default=no@:>@])],
              [],
              [enable_hwloc=no])
case "$enable_hwloc" in
   no)
      ;;
   yes)
      AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [AC_MSG_ERROR([can not find required library libhwloc])])
      AC_CHECK_HEADERS([hwloc.h], [], [AC_MSG_ERROR([can not find require header file hwloc.h])])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_hwloc' for --enable-hwloc])
      ;;
esac

AC_ARG_WITH([os-release],
            [AS_HELP_STRING([--with-os-release=FILE],
                            [location of an os-release file @<:@default=/etc/os-release@:>@])],
            [],
            [with_os_release=/etc/os-release])
if test -n "$with_os_release" && test ! -f "$with_os_release"; then
   if test -f "/usr/lib/os-release"; then
      with_os_release="/usr/lib/os-release"
   fi
fi
AC_DEFINE_UNQUOTED([OSRELEASEFILE], ["$with_os_release"], [File with OS release details.])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for Linux features and flags.
# ----------------------------------------------------------------------

AC_ARG_WITH([proc],
            [AS_HELP_STRING([--with-proc=DIR],
                            [location of a Linux-compatible proc filesystem @<:@default=/proc@:>@])],
            [],
            [with_proc=/proc])
if test -z "$with_proc"; then
   AC_MSG_ERROR([bad empty value for --with-proc option])
fi
AC_DEFINE_UNQUOTED([PROCDIR], ["$with_proc"], [Path of proc filesystem.])


AC_ARG_ENABLE([openvz],
              [AS_HELP_STRING([--enable-openvz],
                              [enable OpenVZ support @<:@default=no@:>@])],
              [],
              [enable_openvz=no])
if test "x$enable_openvz" = xyes; then
   AC_DEFINE([HAVE_OPENVZ], [1], [Define if openvz support enabled.])
fi


AC_ARG_ENABLE([vserver],
              [AS_HELP_STRING([--enable-vserver],
                              [enable VServer support @<:@default=no@:>@])],
              [],
              [enable_vserver=no])
if test "x$enable_vserver" = xyes; then
    AC_DEFINE([HAVE_VSERVER], [1], [Define if VServer support enabled.])
fi


AC_ARG_ENABLE([ancient_vserver],
              [AS_HELP_STRING([--enable-ancient-vserver],
                              [enable ancient VServer support (implies --enable-vserver) @<:@default=no@:>@])],
              [],
              [enable_ancient_vserver=no])
if test "x$enable_ancient_vserver" = xyes; then
    AC_DEFINE([HAVE_VSERVER], [1], [Define if VServer support enabled.])
    AC_DEFINE([HAVE_ANCIENT_VSERVER], [1], [Define if ancient vserver support enabled.])
fi


AC_ARG_ENABLE([linux_affinity],
              [AS_HELP_STRING([--enable-linux-affinity],
                              [enable Linux sched_setaffinity and sched_getaffinity for affinity support, conflicts with hwloc @<:@default=check@:>@])],
              [],
              [enable_linux_affinity=check])
if test "x$enable_linux_affinity" = xcheck; then
   if test "x$enable_hwloc" = xyes; then
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
      [enable_linux_affinity=yes
         AC_MSG_RESULT([yes])],
      [enable_linux_affinity=no
         AC_MSG_RESULT([no])],
      [AC_MSG_RESULT([yes (assumed while cross compiling)])])
   fi
fi
if test "x$enable_linux_affinity" = xyes; then
   if test "x$enable_hwloc" = xyes; then
      AC_MSG_ERROR([--enable-hwloc and --enable-linux-affinity are mutual exclusive. Specify at most one of them.])
   fi
   AC_DEFINE([HAVE_LINUX_AFFINITY], [1], [Define if Linux sched_setaffinity and sched_getaffinity are to be used.])
fi


AC_ARG_ENABLE([capabilities],
              [AS_HELP_STRING([--enable-capabilities],
                              [enable Linux capabilities support; requires libcap @<:@default=check@:>@])],
              [],
              [enable_capabilities=check])
case "$enable_capabilities" in
   no)
      ;;
   check)
      enable_capabilities=yes
      AC_CHECK_LIB([cap], [cap_init], [], [enable_capabilities=no])
      AC_CHECK_HEADERS([sys/capability.h], [], [enable_capabilities=no])
      ;;
   yes)
      AC_CHECK_LIB([cap], [cap_init], [], [AC_MSG_ERROR([can not find required library libcap])])
      AC_CHECK_HEADERS([sys/capability.h], [], [AC_MSG_ERROR([can not find required header file sys/capability.h])])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_capabilities' for --enable-capabilities])
      ;;
esac


AC_ARG_ENABLE([delayacct],
              [AS_HELP_STRING([--enable-delayacct],
                              [enable Linux delay accounting support; requires pkg-config, libnl-3 and libnl-genl-3 @<:@default=check@:>@])],
              [],
              [enable_delayacct=check])
case "$enable_delayacct" in
   no)
      ;;
   check)
      if test "$my_htop_platform" != linux; then
         enable_delayacct=no
      elif test "$enable_static" = yes; then
         enable_delayacct=no
      else
         m4_ifdef([PKG_PROG_PKG_CONFIG], [
            enable_delayacct=yes
            PKG_PROG_PKG_CONFIG()
            PKG_CHECK_MODULES(LIBNL3, libnl-3.0, [], [enable_delayacct=no])
            PKG_CHECK_MODULES(LIBNL3GENL, libnl-genl-3.0, [], [enable_delayacct=no])
            if test "$enable_delayacct" = yes; then
               CFLAGS="$CFLAGS $LIBNL3_CFLAGS $LIBNL3GENL_CFLAGS"
               LIBS="$LIBS $LIBNL3_LIBS $LIBNL3GENL_LIBS"
               AC_DEFINE([HAVE_DELAYACCT], [1], [Define if delay accounting support should be enabled.])
            fi
         ], [
            enable_delayacct=no
            AC_MSG_NOTICE([Linux delay accounting support can not be enabled, cause pkg-config is required for checking its availability])
         ])
      fi
      ;;
   yes)
      m4_ifdef([PKG_PROG_PKG_CONFIG], [
         PKG_PROG_PKG_CONFIG()
         PKG_CHECK_MODULES(LIBNL3, libnl-3.0, [], [AC_MSG_ERROR([can not find required library libnl3])])
         PKG_CHECK_MODULES(LIBNL3GENL, libnl-genl-3.0, [], [AC_MSG_ERROR([can not find required library libnl3genl])])
         CFLAGS="$CFLAGS $LIBNL3_CFLAGS $LIBNL3GENL_CFLAGS"
         LIBS="$LIBS $LIBNL3_LIBS $LIBNL3GENL_LIBS"
         AC_DEFINE([HAVE_DELAYACCT], [1], [Define if delay accounting support should be enabled.])
      ], [
         pkg_m4_absent=1
         m4_warning([configure is generated without pkg.m4. 'make dist' target will be disabled.])
         AC_MSG_ERROR([htop on Linux requires pkg-config for checking delayacct requirements. Please install pkg-config and run ./autogen.sh to rebuild the configure script.])
      ])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_delayacct' for --enable-delayacct])
      ;;
esac


AC_ARG_ENABLE([sensors],
              [AS_HELP_STRING([--enable-sensors],
                              [enable libsensors support for reading temperature data; requires only libsensors headers at compile time, at runtime libsensors is loaded via dlopen @<:@default=check@:>@])],
              [],
              [enable_sensors=check])
case "$enable_sensors" in
   no)
      ;;
   check)
      enable_sensors=yes
      if test "$enable_static" = yes; then
         AC_CHECK_LIB([sensors], [sensors_init], [], [enable_sensors=no])
      fi
      AC_CHECK_HEADERS([sensors/sensors.h], [], [enable_sensors=no])
      ;;
   yes)
      if test "$enable_static" = yes; then
         AC_CHECK_LIB([sensors], [sensors_init], [], [AC_MSG_ERROR([can not find required library libsensors])])
      fi
      AC_CHECK_HEADERS([sensors/sensors.h], [], [AC_MSG_ERROR([can not find required header file sensors/sensors.h])])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_sensors' for --enable-sensors])
      ;;
esac
if test "$enable_sensors" = yes || test "$my_htop_platform" = freebsd; then
   AC_DEFINE([BUILD_WITH_CPU_TEMP], [1], [Define if CPU temperature option should be enabled.])
fi

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
 -Winit-self\
 -Wmissing-format-attribute\
 -Wmissing-noreturn\
 -Wmissing-prototypes\
 -Wpointer-arith\
 -Wshadow\
 -Wstrict-prototypes\
 -Wundef\
 -Wunused\
 -Wwrite-strings"

# FreeBSD uses C11 _Generic in its isnan implementation, even with -std=c99
if test "$my_htop_platform" = freebsd; then
   AM_CFLAGS="$AM_CFLAGS -Wno-c11-extensions"
fi

dnl https://www.gnu.org/software/autoconf-archive/ax_check_compile_flag.html
AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
   ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
   _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
   AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
      [AS_VAR_SET(CACHEVAR,[yes])],
      [AS_VAR_SET(CACHEVAR,[no])])
   _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
   [m4_default([$2], :)],
   [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

AX_CHECK_COMPILE_FLAG([-Wnull-dereference], [AM_CFLAGS="$AM_CFLAGS -Wnull-dereference"], , [-Werror])

AC_ARG_ENABLE([werror],
              [AS_HELP_STRING([--enable-werror],
                              [Treat warnings as errors @<:@default=no@:>@])],
              [],
              [enable_werror=no])
if test "x$enable_werror" = xyes; then
   AM_CFLAGS="$AM_CFLAGS -Werror"
fi

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug],
                              [Enable compiling with maximum debug info, asserts and internal sanity checks @<:@default=no@:>@])],
              [],
              [enable_debug=no])
if test "x$enable_debug" != xyes; then
   AM_CPPFLAGS="$AM_CPPFLAGS -DNDEBUG"
else
   AM_CPPFLAGS="$AM_CPPFLAGS -ggdb3"
fi


AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# We're done, let's go!
# ----------------------------------------------------------------------

AC_DEFINE_UNQUOTED([COPYRIGHT], ["(C) 2004-2019 Hisham Muhammad. (C) 2020-2021 htop dev team."], [Copyright message.])

AM_CONDITIONAL([HTOP_LINUX], [test "$my_htop_platform" = linux])
AM_CONDITIONAL([HTOP_FREEBSD], [test "$my_htop_platform" = freebsd])
AM_CONDITIONAL([HTOP_DRAGONFLYBSD], [test "$my_htop_platform" = dragonflybsd])
AM_CONDITIONAL([HTOP_OPENBSD], [test "$my_htop_platform" = openbsd])
AM_CONDITIONAL([HTOP_DARWIN], [test "$my_htop_platform" = darwin])
AM_CONDITIONAL([HTOP_SOLARIS], [test "$my_htop_platform" = solaris])
AM_CONDITIONAL([HTOP_PCP], [test "$my_htop_platform" = pcp])
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
  os-release file:           $with_os_release
  (Linux) proc directory:    $with_proc
  (Linux) openvz:            $enable_openvz
  (Linux) vserver:           $enable_vserver
  (Linux) ancient vserver:   $enable_ancient_vserver
  (Linux) affinity:          $enable_linux_affinity
  (Linux) delay accounting:  $enable_delayacct
  (Linux) sensors:           $enable_sensors
  (Linux) capabilities:      $enable_capabilities
  unicode:                   $enable_unicode
  hwloc:                     $enable_hwloc
  debug:                     $enable_debug
  static:                    $enable_static
])
