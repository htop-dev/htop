#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.65)
LT_PREREQ([2.4.0])
AC_INIT([htop],[2.0.2],[hisham@gobolinux.org])

SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"
year=$(date -u -d "@$SOURCE_DATE_EPOCH" "+%Y" 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" "+%Y" 2>/dev/null || date -u "+%Y")

AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Required by hwloc scripts
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11])

# Checks for programs.
# ----------------------------------------------------------------------
AC_PROG_CC
AM_PROG_CC_C_O

# Required by hwloc scripts
AC_USE_SYSTEM_EXTENSIONS

LT_INIT([disable-shared static])

# Checks for platform.
# ----------------------------------------------------------------------
case "$target_os" in
linux*|gnu*)
   my_htop_platform=linux
   ;;
freebsd*|kfreebsd*)
   my_htop_platform=freebsd
   ;;
openbsd*)
   my_htop_platform=openbsd
   ;;
darwin*)
   my_htop_platform=darwin
   ;;
*)
   my_htop_platform=unsupported
   ;;
esac

# Checks for libraries.
# ----------------------------------------------------------------------
AC_CHECK_LIB([m], [ceil], [], [missing_libraries="$missing_libraries libm"])

# Checks for header files.
# ----------------------------------------------------------------------
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h strings.h sys/param.h sys/time.h unistd.h],[:],[
  missing_headers="$missing_headers $ac_header"
])
AC_CHECK_HEADERS([execinfo.h],[:],[:])

# Checks for typedefs, structures, and compiler characteristics.
# ----------------------------------------------------------------------
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_UID_T

# Checks for library functions and compiler features.
# ----------------------------------------------------------------------
AC_FUNC_CLOSEDIR_VOID
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove strncasecmp strstr strdup])

save_cflags="${CFLAGS}"
CFLAGS="${CFLAGS} -std=c99"
AC_MSG_CHECKING([whether gcc -std=c99 option works])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
   [AC_INCLUDES_DEFAULT], [[char *a; a = strdup("foo"); int i = 0; i++; // C99]])],
   [AC_MSG_RESULT([yes])],
   [AC_MSG_ERROR([htop is written in C99. A newer version of gcc is required.])])
CFLAGS="$save_cflags"

save_cflags="${CFLAGS}"
CFLAGS="$CFLAGS -Wextra"
AC_MSG_CHECKING([if compiler supports -Wextra])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],[
   wextra_flag=-Wextra
   AC_MSG_RESULT([yes])
],[
   wextra_flag=
   AC_MSG_RESULT([no])
])
CFLAGS="$save_cflags"
AC_SUBST(wextra_flag)

# Checks for features and flags.
# ----------------------------------------------------------------------
PROCDIR=/proc

AC_ARG_ENABLE(proc, [AS_HELP_STRING([--enable-proc], [use Linux-compatible proc filesystem])], enable_proc="yes", enable_proc="no")
if test "x$enable_proc" = xyes; then
   # An enabled proc assumes we're emulating Linux.
   my_htop_platform=linux
   AC_DEFINE(HAVE_PROC, 1, [Define if using a Linux-compatible proc filesystem.])
fi

AC_ARG_WITH(proc, [AS_HELP_STRING([--with-proc=DIR], [Location of a Linux-compatible proc filesystem (default=/proc).])],

if test -n "$withval"; then
   AC_DEFINE_UNQUOTED(PROCDIR, "$withval", [Path of proc filesystem])
   PROCDIR="$withval"
fi,
AC_DEFINE(PROCDIR, "/proc", [Path of proc filesystem]))

if test "x$cross_compiling" = xno; then
   if test "x$enable_proc" = xyes; then
      AC_CHECK_FILE($PROCDIR/stat,,AC_MSG_ERROR(Cannot find $PROCDIR/stat. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
      AC_CHECK_FILE($PROCDIR/meminfo,,AC_MSG_ERROR(Cannot find $PROCDIR/meminfo. Make sure you have a Linux-compatible /proc filesystem mounted. See the file README for help.))
   fi
fi

AC_ARG_ENABLE(openvz, [AS_HELP_STRING([--enable-openvz], [enable OpenVZ support])], ,enable_openvz="no")
if test "x$enable_openvz" = xyes; then
   AC_DEFINE(HAVE_OPENVZ, 1, [Define if openvz support enabled.])
fi

AC_ARG_ENABLE(cgroup, [AS_HELP_STRING([--enable-cgroup], [enable cgroups support])], ,enable_cgroup="no")
if test "x$enable_cgroup" = xyes; then
   AC_DEFINE(HAVE_CGROUP, 1, [Define if cgroup support enabled.])
fi

AC_ARG_ENABLE(vserver, [AS_HELP_STRING([--enable-vserver], [enable VServer support])], ,enable_vserver="no")
if test "x$enable_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
fi

AC_ARG_ENABLE(ancient_vserver, [AS_HELP_STRING([--enable-ancient-vserver], [enable ancient VServer support (implies --enable-vserver)])], ,enable_ancient_vserver="no")
if test "x$enable_ancient_vserver" = xyes; then
    AC_DEFINE(HAVE_VSERVER, 1, [Define if vserver support enabled.])
    AC_DEFINE(HAVE_ANCIENT_VSERVER, 1, [Define if ancient vserver support enabled.])
fi

AC_ARG_ENABLE(taskstats, [AS_HELP_STRING([--enable-taskstats], [enable per-task IO Stats (taskstats kernel sup required)])], ,enable_taskstats="yes")
if test "x$enable_taskstats" = xyes; then
    AC_DEFINE(HAVE_TASKSTATS, 1, [Define if taskstats support enabled.])
fi

# HTOP_CHECK_SCRIPT(LIBNAME, FUNCTION, DEFINE, CONFIG_SCRIPT, ELSE_PART)
m4_define([HTOP_CHECK_SCRIPT],
[
   if test ! -z "m4_toupper($HTOP_[$1]_CONFIG_SCRIPT)"; then
      # to be used to set the path to ncurses*-config when cross-compiling
      htop_config_script=$(m4_toupper($HTOP_[$1]_CONFIG_SCRIPT) --libs 2> /dev/null)
   else
      htop_config_script=$([$4] --libs 2> /dev/null)
   fi
   htop_script_success=no
   htop_save_LDFLAGS="$LDFLAGS"
   if test ! "x$htop_config_script" = x; then
      LDFLAGS="$htop_config_script $LDFLAGS"
      AC_CHECK_LIB([$1], [$2], [
         AC_DEFINE([$3], 1, [The library is present.])
         LIBS="$htop_config_script $LIBS "
         htop_script_success=yes
      ], [])
      LDFLAGS="$save_LDFLAGS"
   fi
   if test "x$htop_script_success" = xno; then
      [$5]
   fi
])

# HTOP_CHECK_LIB(LIBNAME, FUNCTION, DEFINE, ELSE_PART)
m4_define([HTOP_CHECK_LIB],
[
   AC_CHECK_LIB([$1], [$2], [
      AC_DEFINE([$3], 1, [The library is present.])
      LIBS="-l[$1] $LIBS "
   ], [$4])
])

AC_ARG_ENABLE(unicode, [AS_HELP_STRING([--enable-unicode], [enable Unicode support])], ,enable_unicode="yes")
if test "x$enable_unicode" = xyes; then
   HTOP_CHECK_SCRIPT([ncursesw6], [addnwstr], [HAVE_LIBNCURSESW], "ncursesw6-config",
    HTOP_CHECK_SCRIPT([ncursesw], [addnwstr], [HAVE_LIBNCURSESW], "ncursesw5-config",
     HTOP_CHECK_SCRIPT([ncurses], [addnwstr], [HAVE_LIBNCURSESW], "ncurses5-config",
      HTOP_CHECK_LIB([ncursesw6], [addnwstr], [HAVE_LIBNCURSESW],
       HTOP_CHECK_LIB([ncursesw], [addnwstr], [HAVE_LIBNCURSESW],
        HTOP_CHECK_LIB([ncurses], [addnwstr], [HAVE_LIBNCURSESW],
      missing_libraries="$missing_libraries libncursesw"
      AC_MSG_ERROR([You may want to use --disable-unicode or install libncursesw.])
   ))))))

   AC_CHECK_HEADERS([ncursesw/curses.h],[:],
      [AC_CHECK_HEADERS([ncurses/ncurses.h],[:],
         [AC_CHECK_HEADERS([ncurses/curses.h],[:],
            [AC_CHECK_HEADERS([ncurses.h],[:],[missing_headers="$missing_headers $ac_header"])])])])
else
   HTOP_CHECK_SCRIPT([ncurses6], [refresh], [HAVE_LIBNCURSES], "ncurses6-config",
    HTOP_CHECK_SCRIPT([ncurses], [refresh], [HAVE_LIBNCURSES], "ncurses5-config",
     HTOP_CHECK_LIB([ncurses6],  [refresh], [HAVE_LIBNCURSES],
      HTOP_CHECK_LIB([ncurses],  [refresh], [HAVE_LIBNCURSES],
      missing_libraries="$missing_libraries libncurses"
   ))))
   
   AC_CHECK_HEADERS([curses.h],[:],
      [AC_CHECK_HEADERS([ncurses/curses.h],[:],
         [AC_CHECK_HEADERS([ncurses/ncurses.h],[:],
            [AC_CHECK_HEADERS([ncurses.h],[:],[missing_headers="$missing_headers $ac_header"])])])])
fi

if test "$my_htop_platform" = "freebsd"; then
   AC_CHECK_LIB([kvm], [kvm_open], [], [missing_libraries="$missing_libraries libkvm"])
fi

if test "$my_htop_platform" = "openbsd"; then
   AC_CHECK_LIB([kvm], [kvm_open], [], [missing_libraries="$missing_libraries libkvm"])
fi

AC_ARG_ENABLE(linux_affinity, [AS_HELP_STRING([--enable-linux-affinity], [enable Linux sched_setaffinity and sched_getaffinity for affinity support, disables hwloc])], ,enable_linux_affinity="yes")
if test "x$enable_linux_affinity" = xyes -a "x$cross_compiling" = xno; then
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
     [enable_linux_affinity=no
      AC_MSG_RESULT([no])])
fi
if test "x$enable_linux_affinity" = xyes; then
   AC_DEFINE(HAVE_LINUX_AFFINITY, 1, [Define if Linux sched_setaffinity and sched_getaffinity are to be used.])
fi

AC_ARG_ENABLE(hwloc, [AS_HELP_STRING([--enable-hwloc], [enable hwloc support for CPU affinity])],, enable_hwloc="no")
if test "x$enable_hwloc" = xyes
then
   AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [missing_libraries="$missing_libraries libhwloc"])
   AC_CHECK_HEADERS([hwloc.h],[:], [missing_headers="$missing_headers $ac_header"])
fi

# Bail out on errors.
# ----------------------------------------------------------------------
if test ! -z "$missing_libraries"; then
  AC_MSG_ERROR([missing libraries: $missing_libraries])
fi
if test ! -z "$missing_headers"; then
  AC_MSG_ERROR([missing headers: $missing_headers])
fi

AC_DEFINE_UNQUOTED(COPYRIGHT, "(C) 2004-$year Hisham Muhammad", [Copyright message.])

# We're done, let's go!
# ----------------------------------------------------------------------
AM_CONDITIONAL([HTOP_LINUX], [test "$my_htop_platform" = linux])
AM_CONDITIONAL([HTOP_FREEBSD], [test "$my_htop_platform" = freebsd])
AM_CONDITIONAL([HTOP_OPENBSD], [test "$my_htop_platform" = openbsd])
AM_CONDITIONAL([HTOP_DARWIN], [test "$my_htop_platform" = darwin])
AM_CONDITIONAL([HTOP_UNSUPPORTED], [test "$my_htop_platform" = unsupported])
AC_SUBST(my_htop_platform)
AC_CONFIG_FILES([Makefile htop.1])
AC_OUTPUT
