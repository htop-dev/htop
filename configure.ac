#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.65)
AC_INIT([htop],[1.0.2],[loderunner@users.sourceforge.net])

# The following two lines are required by hwloc scripts
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11])
AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

AC_DISABLE_SHARED
AC_ENABLE_STATIC
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([m], [ceil], [], [missing_libraries="$missing_libraries libm"])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h strings.h sys/param.h sys/time.h unistd.h curses.h],[:],[
  missing_headers="$missing_headers $ac_header"
])
AC_CHECK_HEADERS([execinfo.h],[:],[:])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove strncasecmp strstr strdup])

save_cflags="${CFLAGS}"
CFLAGS="${CFLAGS} -std=c99"
AC_MSG_CHECKING([whether gcc -std=c99 option works])
AC_TRY_COMPILE(AC_INCLUDES_DEFAULT, [char *a; a = strdup("foo"); int i = 0; i++; // C99],
   AC_MSG_RESULT([yes]),
   AC_MSG_ERROR([htop is written in C99. A newer version of gcc is required.]))
CFLAGS="$save_cflags"

PROCDIR=/proc
AC_ARG_WITH(proc, [  --with-proc=DIR      Location of a Linux-compatible proc filesystem (default=/proc).],

   if test -n "$withval"; then
      AC_DEFINE_UNQUOTED(PROCDIR, "$withval", [Path of proc filesystem])
      PROCDIR="$withval"
   fi,
   AC_DEFINE(PROCDIR, "/proc", [Path of proc filesystem]))

AC_ARG_ENABLE(openvz, [AC_HELP_STRING([--enable-openvz], [enable OpenVZ support])], ,enable_openvz="no")
if test "x$enable_openvz" = xyes; then
   AC_DEFINE(HAVE_OPENVZ, 1, [Define if openvz support enabled.])
fi

AC_ARG_ENABLE(cgroup, [AC_HELP_STRING([--enable-cgroup], [enable cgroups support])], ,enable_cgroup="no")
if test "x$enable_cgroup" = xyes; then
   AC_DEFINE(HAVE_CGROUP, 1, [Define if cgroup support enabled.])
fi

AC_ARG_ENABLE(vserver, [AC_HELP_STRING([--enable-vserver], [enable VServer support])], ,enable_vserver="no")
if test "x$enable_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
fi

AC_ARG_ENABLE(ancient_vserver, [AC_HELP_STRING([--enable-ancient-vserver], [enable ancient VServer support (implies --enable-vserver)])], ,enable_ancient_vserver="no")
if test "x$enable_ancient_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
    AC_DEFINE(HAVE_ANCIENT_VSERVER, 1, [Define if ancient vserver support enabled.])
fi

AC_ARG_ENABLE(taskstats, [AC_HELP_STRING([--enable-taskstats], [enable per-task IO Stats (taskstats kernel sup required)])], ,enable_taskstats="yes")
if test "x$enable_taskstats" = xyes; then
    AC_DEFINE(HAVE_TASKSTATS, 1, [Define if taskstats support enabled.])
fi

AC_ARG_ENABLE(unicode, [AC_HELP_STRING([--enable-unicode], [enable Unicode support])], ,enable_unicode="yes")
if test "x$enable_unicode" = xyes; then
   AC_CHECK_LIB([ncursesw], [refresh], [], [
      missing_libraries="$missing_libraries libncursesw"
      AC_MSG_ERROR([You may want to use --disable-unicode or install libncursesw.])
   ])
   AC_CHECK_HEADERS([ncursesw/curses.h],[:],
        [AC_CHECK_HEADERS([ncurses/ncurses.h],[:],
          [AC_CHECK_HEADERS([ncurses.h],[:],[missing_headers="$missing_headers $ac_header"])])])
else
   AC_CHECK_LIB([ncurses], [refresh], [], [missing_libraries="$missing_libraries libncurses"])
   AC_CHECK_HEADERS([curses.h],[:],[missing_headers="$missing_headers $ac_header"])
fi

if test ! -z "$missing_libraries"; then
  AC_MSG_ERROR([missing libraries: $missing_libraries])
fi
if test ! -z "$missing_headers"; then
  AC_MSG_ERROR([missing headers: $missing_headers])
fi

if test "x$cross_compiling" = xno; then
AC_CHECK_FILE($PROCDIR/stat,,AC_MSG_ERROR(Cannot find /proc/stat. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
AC_CHECK_FILE($PROCDIR/meminfo,,AC_MSG_ERROR(Cannot find /proc/meminfo. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
fi

AC_ARG_ENABLE(native_affinity, [AC_HELP_STRING([--enable-native-affinity], [enable native sched_setaffinity and sched_getaffinity for affinity support, disables hwloc])], ,enable_native_affinity="yes")
if test "x$enable_native_affinity" = xyes -a "x$cross_compiling" = xno; then
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
     [AC_MSG_RESULT([yes])],
     [enable_native_affinity=no
      AC_MSG_RESULT([no])])
fi
if test "x$enable_native_affinity" = xyes; then
   AC_DEFINE(HAVE_NATIVE_AFFINITY, 1, [Define if native sched_setaffinity and sched_getaffinity are to be used.])
fi

AC_ARG_ENABLE(hwloc, [AC_HELP_STRING([--enable-hwloc], [enable hwloc support for CPU affinity])],, enable_hwloc="no")
if test "x$enable_hwloc" = xyes
then
   AC_CHECK_LIB([hwloc], [hwloc_linux_get_tid_cpubind], [], [missing_libraries="$missing_libraries libhwloc"])
   AC_CHECK_HEADERS([hwloc.h],[:], [missing_headers="$missing_headers $ac_header"])
fi

AC_CONFIG_FILES([Makefile htop.1])
AC_OUTPUT
