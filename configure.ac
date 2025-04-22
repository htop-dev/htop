#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# ----------------------------------------------------------------------
# Build version string.
# ----------------------------------------------------------------------

m4_define([htop_git_version],
  m4_normalize(m4_esyscmd([git describe --abbrev=7 --dirty --always --tags 2> /dev/null || echo ''])))
m4_define([htop_release_version], [3.5.0-dev])

# ----------------------------------------------------------------------
# Autoconf initialization.
# ----------------------------------------------------------------------

AC_PREREQ([2.69])
AC_INIT([htop], [m4_join([-],m4_defn([htop_release_version]),m4_defn([htop_git_version]))], [htop@groups.io], [], [https://htop.dev/])

AC_CONFIG_SRCDIR([htop.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([-Wall std-options subdir-objects])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for platform.
# ----------------------------------------------------------------------

case "$host_os" in
linux*|gnu*)
   my_htop_platform=linux
   AC_DEFINE([HTOP_LINUX], [], [Building for Linux.])
   ;;
freebsd*|kfreebsd*)
   my_htop_platform=freebsd
   AC_DEFINE([HTOP_FREEBSD], [], [Building for FreeBSD.])
   ;;
netbsd*)
   my_htop_platform=netbsd
   AC_DEFINE([HTOP_NETBSD], [], [Building for NetBSD.])
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

# The header of ncurses exposes wide-character interfaces only if
# _XOPEN_SOURCE_EXTENDED is defined or _XOPEN_SOURCE defined to be
# >= 500. In DragonFly BSD, FreeBSD and OpenBSD, defining
# _XOPEN_SOURCE to any value *hides* interfaces that would be useful
# for htop.
dnl
dnl Note: The "#undef" in AH_VERBATIM will be replaced with "#define"
dnl when config.h is generated.
AH_VERBATIM([_XOPEN_SOURCE_EXTENDED],
[/* Enables XPG4v2 (SUSv1) interfaces if they are not enabled already with _XOPEN_SOURCE */
#ifndef _XOPEN_SOURCE_EXTENDED
#undef _XOPEN_SOURCE_EXTENDED
#endif
])
AC_DEFINE([_XOPEN_SOURCE_EXTENDED], 1)

# Activate some more of the missing global defines
AC_SYS_LARGEFILE

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for compiler.
# ----------------------------------------------------------------------

AC_PROG_CC
AM_PROG_CC_C_O
m4_version_prereq([2.70], [], [AC_PROG_CC_C99])
AS_IF([test "x$ac_cv_prog_cc_c99" = xno], [AC_MSG_ERROR([htop is written in C99. A newer compiler is required.])])
AM_CFLAGS="-std=c99 -pedantic"

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
      AC_CONFIG_FILES([pcp-htop.5])
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
m4_version_prereq([2.70], [AC_CHECK_INCLUDES_DEFAULT], [AC_HEADER_STDC])
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
      [Define to 1 if 'major', 'minor', and 'makedev' are declared in <sys/sysmacros.h>.])])
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

AC_TYPE_MBSTATE_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_MSG_CHECKING(for alloc_size)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-error -Werror=attributes"
AC_COMPILE_IFELSE([
   AC_LANG_SOURCE(
      [
         /* Attribute supported in GCC 4.3 or later */
         __attribute__((alloc_size(1))) char* my_alloc(int size) { return 0; }
      ],[]
   )],
   AC_DEFINE([HAVE_ATTR_ALLOC_SIZE], 1, [The alloc_size attribute is supported.])
   AC_MSG_RESULT(yes),
   AC_MSG_RESULT(no))
CFLAGS="$old_CFLAGS"

AC_MSG_CHECKING(for access)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-error -Werror=attributes"
AC_COMPILE_IFELSE([
   AC_LANG_SOURCE(
      [
         /* Attribute supported in GCC 10 or later */
         __attribute__((access(read_only, 1, 2))) extern int foo(const char* str, unsigned len);
      ],[]
   )],
   AC_DEFINE([HAVE_ATTR_ACCESS], 1, [The access attribute is supported.])
   AC_MSG_RESULT(yes),
   AC_MSG_RESULT(no))
CFLAGS="$old_CFLAGS"

AC_MSG_CHECKING(for nonnull)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-error -Werror=attributes"
AC_COMPILE_IFELSE([
   AC_LANG_SOURCE(
      [[
         /* Attribute supported in GCC 3.3 or later */
         __attribute__((nonnull)) int my_strcmp(const char* a, const char* b);
         __attribute__((nonnull(1))) long my_strtol(const char* str, char** endptr, int base);
      ]]
   )],
   AC_DEFINE([HAVE_ATTR_NONNULL], 1, [The nonnull attribute is supported.])
   AC_MSG_RESULT(yes),
   AC_MSG_RESULT(no))
CFLAGS="$old_CFLAGS"

AC_MSG_CHECKING(for returns_nonnull)
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-error -Werror=attributes"
AC_COMPILE_IFELSE([
   AC_LANG_SOURCE(
      [[
         /* Attribute supported in GCC 4.9 or later */
         __attribute__((returns_nonnull)) void* foo(void);
      ]]
   )],
   AC_DEFINE([HAVE_ATTR_RETNONNULL], 1, [The returns_nonnull attribute is supported.])
   AC_MSG_RESULT(yes),
   AC_MSG_RESULT(no))
CFLAGS="$old_CFLAGS"

AC_MSG_CHECKING(for NaN support)
dnl Note: AC_RUN_IFELSE does not try compiling the program at all when
dnl $cross_compiling is 'yes'.
AC_LINK_IFELSE([
   AC_LANG_PROGRAM(
      [[
#include <math.h>
      ]],
      [[
         double x = NAN;
         /* Both should evaluate to false -> 0 (exit success) */
         return isgreater(x, x) || isgreaterequal(x, x);
      ]]
   )],
   [flag_finite_math_only=unknown
   if test "$cross_compiling" = yes; then
      AC_COMPILE_IFELSE([
         AC_LANG_SOURCE([[
/* __FINITE_MATH_ONLY__ is documented in Clang. */
#ifdef __FINITE_MATH_ONLY__
#error "should not enable -ffinite-math-only"
#endif
         ]])],
         AC_MSG_RESULT([assume yes (cross compiling)]),
         flag_finite_math_only=yes)
   elif ./conftest$EXEEXT >&AS_MESSAGE_LOG_FD; then
      flag_finite_math_only=no
      AC_MSG_RESULT(yes)
   else
      flag_finite_math_only=yes
   fi
   if test "$flag_finite_math_only" = yes; then
      AC_MSG_RESULT(no)
      AC_MSG_WARN([runtime behavior with NaN is not compliant - some functionality might break; consider using '-fno-finite-math-only'])
   fi],
   [AC_MSG_RESULT(no)
   AC_MSG_ERROR([can not find required macros: NAN, isgreater() and isgreaterequal()])])

AC_MSG_CHECKING(for __builtin_ctz)
AC_COMPILE_IFELSE([
   AC_LANG_PROGRAM([], [[__builtin_ctz(1); /* Supported in GCC 3.4 or later */]])],
   [AC_DEFINE([HAVE_BUILTIN_CTZ], 1, [Define to 1 if the compiler supports '__builtin_ctz' function.])
   AC_MSG_RESULT(yes)],
   AC_MSG_RESULT(no))

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for generic library functions.
# ----------------------------------------------------------------------

AC_SEARCH_LIBS([ceil], [m], [], [AC_MSG_ERROR([can not find required function ceil()])])

if test "$my_htop_platform" = dragonflybsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([can not find required function kvm_open()])])
   AC_SEARCH_LIBS([getdevs], [devstat], [], [AC_MSG_ERROR([can not find required function getdevs()])])
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

if test "$my_htop_platform" = netbsd; then
   AC_SEARCH_LIBS([kvm_open], [kvm], [], [AC_MSG_ERROR([can not find required function kvm_open()])])
   AC_SEARCH_LIBS([prop_dictionary_get], [prop], [], [AC_MSG_ERROR([can not find required function prop_dictionary_get()])])
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
    dladdr \
    faccessat \
    fstatat \
    host_get_clock_service \
    memfd_create\
    openat \
    readlinkat \
    sched_getscheduler \
    sched_setscheduler \
   ])

# strchrnul is available in macOS since 15.4, but the user may specify an older
# macOS version ('-mmacos-version-min') to build for. We need to ensure it is
# actually exposed in the header.
htop_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Werror"

AC_MSG_CHECKING([for strchrnul])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <string.h>
   ]], [[
static char ch;
char* ptr = strchrnul(&ch, 0);
return ptr != &ch; /* Should be 0 (exit success) */
   ]])],
   [AC_MSG_RESULT(yes)
   AC_DEFINE([HAVE_STRCHRNUL], [1], [Define to 1 if you have the 'strchrnul' function.])],
   [AC_MSG_RESULT(no)])

CFLAGS=$htop_save_CFLAGS

if test "$my_htop_platform" = darwin; then
   AC_CHECK_FUNCS([host_statistics64], [
      AC_CHECK_TYPES([struct vm_statistics64], [], [], [[#include <mach/vm_statistics.h>]])
   ], [])
fi

if test "$my_htop_platform" = pcp; then
   AC_CHECK_FUNCS([pmLookupDescs])
fi

if test "$my_htop_platform" = linux && test "x$enable_static" = xyes; then
   AC_CHECK_LIB([systemd], [sd_bus_open_system], [], [], [-lcap])
fi

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# Checks for cross-platform features and flags.
# ----------------------------------------------------------------------

dnl PKG_PROG_PKG_CONFIG initializes $PKG_CONFIG and related variables.
dnl If the macro is not called, some pkg-config checks might be skipped
dnl and $PKG_CONFIG might be unset.
m4_ifdef([PKG_PROG_PKG_CONFIG], [
   PKG_PROG_PKG_CONFIG()
   pkg_m4_included=1 # Makefile might grep this keyword. Don't remove.
], [
   m4_warn(
      [syntax],
      [pkg.m4 is absent or older than version 0.16; this 'configure' would have incomplete pkg-config support])
])

AC_ARG_ENABLE([unicode],
              [AS_HELP_STRING([--enable-unicode],
                              [enable Unicode support @<:@default=yes@:>@])],
              [],
              [enable_unicode=yes])

AC_ARG_VAR([CURSES_CFLAGS], [C compiler flags for curses; this overrides auto detected values])
AC_ARG_VAR([CURSES_LIBS], [linker flags for curses; this overrides auto detected values])

curses_pkg_names="ncurses6 ncurses5 ncurses ncursest6 ncursest5 \
   ncursest curses"

if test "x$enable_unicode" = xyes; then
   curses_pkg_names="ncursesw6 ncursesw5 ncursesw ncursestw6 ncursestw5 \
      ncursestw $curses_pkg_names"
fi

AC_ARG_WITH([curses],
            [AS_HELP_STRING([--with-curses=NAME],
                            [select curses package NAME to link with; e.g. ncursesw6])],
            [],
            [with_curses=check])
case $with_curses in
check|yes)
   : # No-op. Use default list.
   ;;
no)
   AC_MSG_ERROR([bad value '$with_curses' for --with-curses option])
   ;;
*)
   if test "x${CURSES_CFLAGS+y}${CURSES_LIBS+y}" = xyy; then
      AC_MSG_WARN([ignoring --with-curses value due to override])
   fi
   curses_pkg_names=`echo "x$with_curses" | sed 's/^x\(lib\)\{0,1\}//'`
   ;;
esac

# $1: C preprocessor and compiler flags for curses library
# $2: linker flags for curses library
htop_check_curses_capability () {
   htop_curses_cflags=${CURSES_CFLAGS-"$1"}
   htop_curses_libs=${CURSES_LIBS-"$2"}

   echo "curses cflags${CURSES_CFLAGS+ (override)}: $htop_curses_cflags" >&AS_MESSAGE_LOG_FD
   echo "curses libs${CURSES_LIBS+ (override)}: $htop_curses_libs" >&AS_MESSAGE_LOG_FD

   htop_msg_linker_flags=$htop_curses_libs
   if test "`echo x $htop_msg_linker_flags`" = x; then
      htop_msg_linker_flags="(no linker flags)"
   fi

   htop_curses_status=0 # 0 for success; nonzero for failure

   htop_save_CFLAGS=$CFLAGS
   htop_save_LIBS=$LIBS
   CFLAGS="$AM_CFLAGS $htop_curses_cflags $CFLAGS"
   LIBS="$htop_curses_libs $LIBS"

   # At this point we have not checked the name of curses header, so
   # use forward declaration for the linking tests below.

   # htop uses keypad(), but for ncurses implementation, the symbol is
   # in "-ltinfo" and not "-lncurses".
   # Check "-ltinfo" symbols first, as libncurses might require
   # explicit "-ltinfo" to link (for internal dependency).
   AC_MSG_CHECKING([for keypad in $htop_msg_linker_flags])
   AC_LINK_IFELSE([AC_LANG_PROGRAM([[
/* int keypad(WINDOW* win, bool enable); */
int keypad(void* win, int enable);
      ]], [[
static char dummy;
keypad((void*)&dummy, 0);
      ]])],
      [AC_MSG_RESULT(yes)],
      [AC_MSG_RESULT(no)
      htop_curses_status=1])

   # htop calls refresh(), which might be implemented as a macro.
   # It is more reliable to test linking with doupdate(), which
   # refresh() would call internally.
   if test "$htop_curses_status" -eq 0; then
      AC_MSG_CHECKING([for doupdate in $htop_msg_linker_flags])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
int doupdate(void);
         ]], [[
doupdate();
         ]])],
         [AC_MSG_RESULT(yes)
         htop_curses_capability=nonwide],
         [AC_MSG_RESULT(no)
         htop_curses_status=1])
   fi

   # htop calls mvadd_wchnstr(), which might be implemented as a macro.
   # It is more reliable to test linking with wadd_wchnstr().
   if test "x$htop_curses_status$enable_unicode" = x0yes; then
      AC_MSG_CHECKING([for wadd_wchnstr in $htop_msg_linker_flags])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
/* int wadd_wchnstr(WINDOW* win, const cchar_t* wchstr, int n); */
int wadd_wchnstr(void* win, const void* wchstr, int n);
         ]], [[
static char dummy1;
static char dummy2;
wadd_wchnstr((void*)&dummy1, (void*)&dummy2, 0);
         ]])],
         [AC_MSG_RESULT(yes)
         htop_curses_capability=wide],
         [AC_MSG_RESULT(no)
         htop_curses_status=1])
   fi

   if test "$htop_curses_status" -eq 0; then
      AM_CFLAGS="$AM_CFLAGS $htop_curses_cflags"
      if test "$htop_curses_capability" = wide; then
         AC_DEFINE([HAVE_LIBNCURSESW], 1, [libncursesw is present])
      else
         AC_DEFINE([HAVE_LIBNCURSES], 1, [libcurses is present])
      fi
   else
      LIBS=$htop_save_LIBS
   fi
   CFLAGS=$htop_save_CFLAGS
   return "$htop_curses_status"
} # htop_check_curses_capability

htop_curses_capability=none

if test "x${CURSES_CFLAGS+y}${CURSES_LIBS+y}" = xyy; then
   curses_pkg_names=""
   htop_check_curses_capability "$CURSES_CFLAGS" "$CURSES_LIBS"
fi

# Prioritize $PKG_CONFIG over ncurses*-config, as users might need to
# cross-compile htop.
if test "x$PKG_CONFIG" != x; then
   pkg_config_static_flag=""
   if test "$enable_static" = yes; then
      pkg_config_static_flag=--static
   fi

   for curses_name in $curses_pkg_names; do
      echo retrieving $curses_name information through $PKG_CONFIG >&AS_MESSAGE_LOG_FD
      $PKG_CONFIG --exists --print-errors $curses_name >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD || continue

      : # Resets "$?" to 0
      htop_config_cflags=`$PKG_CONFIG --cflags $pkg_config_static_flag $curses_name 2>/dev/null` || continue
      : # Resets "$?" to 0
      htop_config_libs=`$PKG_CONFIG --libs $pkg_config_static_flag $curses_name 2>/dev/null` || continue

      AC_MSG_RESULT([found $curses_name information through $PKG_CONFIG])

      if htop_check_curses_capability "$htop_config_cflags" "$htop_config_libs"; then
         break
      fi
   done
fi

case ${htop_curses_capability}-$enable_unicode in
none-*|nonwide-yes)
   for curses_name in $curses_pkg_names; do
      echo retrieving $curses_name information through ${curses_name}-config >&AS_MESSAGE_LOG_FD

      ${curses_name}-config --cflags >/dev/null 2>&AS_MESSAGE_LOG_FD || continue

      : # Resets "$?" to 0
      htop_config_cflags=`${curses_name}-config --cflags 2>/dev/null` || continue
      : # Resets "$?" to 0
      htop_config_libs=`${curses_name}-config --libs 2>/dev/null` || continue

      AC_MSG_RESULT([found $curses_name information through ${curses_name}-config])

      if htop_check_curses_capability "$htop_config_cflags" "$htop_config_libs"; then
         break
      fi
   done
   ;;
esac

case ${htop_curses_capability}-$enable_unicode in
none-*|nonwide-yes)
   if test "x$PKG_CONFIG" = x; then (
      # Friendly warning to advise user to install pkg-config.
      # The local variables get discarded when done.
      list=""
      echo "" >conftest.c
      if test "$cross_compiling" != yes; then
         # "-print-multi-directory" supported in GCC 3.0 or later
         name=""
         if $CC $CPPFLAGS $CFLAGS -print-multi-directory -E conftest.c >/dev/null 2>&1; then
            name=`$CC $CPPFLAGS $CFLAGS -print-multi-directory -E conftest.c 2>/dev/null |
               sed '/^ *#/ d; s/^\.$//; q'`
         fi
         list="/usr/lib${name}/pkgconfig $list"
         case $host_os in
         darwin*)
            list="/opt/homebrew/Library/Homebrew/os/mac/pkgconfig $list"
            ;;
         freebsd*)
            list="/usr/libdata/pkgconfig $list"
            ;;
         netbsd*)
            list="/usr/pkg/lib/pkgconfig $list"
            ;;
         esac
      fi
      if test "x$host_alias" != x; then
         list="/usr/lib/${host_alias}/pkgconfig $list"
      fi
      # "-print-multiarch" supported in GCC 4.6 or later
      if $CC $CPPFLAGS $CFLAGS -print-multiarch -E conftest.c >/dev/null 2>&1; then
         name=`$CC $CPPFLAGS $CFLAGS -print-multiarch -E conftest.c 2>/dev/null |
            sed '/^ *#/ d; q'`
         if test "x$name" != x; then
            list="/usr/lib/${name}/pkgconfig $list"
         fi
      fi
      rm -f conftest.*

      for d in $list; do
         result=`find "$d" -name '*curses*.pc' 2>/dev/null | sed '1 q'`
         if test "x$result" != x; then
            echo detected a .pc file: "$result" >&AS_MESSAGE_LOG_FD
            AC_MSG_WARN([your system supports pkg-config; installing pkg-config is recommended before configuring htop])
            m4_ifdef([PKG_PROG_PKG_CONFIG], [], [
               AC_MSG_WARN([this configure script would also need to be regenerated])
            ])
            break
         fi
      done
   ) fi

   # OpenBSD and Solaris are known to not provide '*curses*.pc' files.
   AC_MSG_RESULT([no curses information found through '*-config' utilities; will guess the linker flags])
   for curses_name in $curses_pkg_names; do
      if htop_check_curses_capability "" "-l$curses_name"; then
         break
      fi
      # For ncurses implementation, an extra terminfo library might be
      # needed. Guess the terminfo library name based on the ncurses
      # library file name (e.g. "-ltinfow" for "-lncursesw"), before
      # trying the "-ltinfo" name.
      tinfo_name=`echo x$curses_name | sed 's/^xncurses/tinfo/p; d'`
      case x$tinfo_name in
      x|xtinfo)
         ;;
      *)
         if htop_check_curses_capability "" "-l$curses_name -l$tinfo_name"; then
            break
         fi
         ;;
      esac
      if htop_check_curses_capability "" "-l$curses_name -ltinfo"; then
         break
      fi
   done
   ;;
esac

case ${htop_curses_capability}-$enable_unicode in
nonwide-yes)
   AC_MSG_ERROR([cannot find required ncursesw library; you may want to use --disable-unicode])
   ;;
none-*)
   AC_MSG_ERROR([cannot find required curses/ncurses library])
   ;;
esac

htop_save_CFLAGS=$CFLAGS
CFLAGS="$AM_CFLAGS $CFLAGS"

if test "x$enable_unicode" = xyes; then
   AC_CHECK_HEADERS([ncursesw/curses.h], [],
      [AC_CHECK_HEADERS([ncurses/ncurses.h], [],
         [AC_CHECK_HEADERS([ncurses/curses.h], [],
            [AC_CHECK_HEADERS([ncurses.h], [],
               [AC_MSG_ERROR([can not find required ncurses header file])])])])])

   AC_CHECK_HEADERS([ncursesw/term.h], [],
      [AC_CHECK_HEADERS([ncurses/term.h], [],
         [AC_CHECK_HEADERS([term.h], [],
            [AC_MSG_ERROR([can not find required term header file])])])])
else
   AC_CHECK_HEADERS([curses.h], [],
      [AC_CHECK_HEADERS([ncurses/curses.h], [],
         [AC_CHECK_HEADERS([ncurses/ncurses.h], [],
            [AC_CHECK_HEADERS([ncurses.h], [],
               [AC_MSG_ERROR([can not find required ncurses header file])])])])])

   AC_CHECK_HEADERS([ncurses/term.h], [],
      [AC_CHECK_HEADERS([term.h], [],
         [AC_MSG_ERROR([can not find required term header file])])])
fi

CFLAGS="-I$srcdir $CFLAGS"

# Check for things that might be macros.
# "stdscr" is a macro in ncursest (reentrant version of ncurses).
AC_MSG_CHECKING([whether the curses header works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
/* Checks if the "bool" definition in curses is ISO C compatible */
#include <stdbool.h>

#include "ProvideCurses.h"
   ]], [[
keypad(stdscr, false);

refresh();

#if defined(HAVE_LIBNCURSESW)
{
   static cchar_t dummy;
   mvadd_wchnstr(0, 0, &dummy, 0);
}
#endif
   ]])],
   [AC_MSG_RESULT(yes)],
   [AC_MSG_RESULT(no)
   AC_MSG_FAILURE([there are problems with the curses header])]
)

CFLAGS=$htop_save_CFLAGS

if test "$enable_static" = yes; then
   AC_SEARCH_LIBS([Gpm_GetEvent], [gpm])
fi
if test "$my_htop_platform" = "solaris"; then
   # On OmniOS /usr/include/sys/regset.h redefines ERR to 13 - \r, breaking the Enter key.
   # Since ncurses macros use the ERR macro, we can not use another name.
   AC_DEFINE([ERR], [(-1)], [Predefine ncurses macro.])
fi
AC_CHECK_FUNCS( [set_escdelay] )
AC_CHECK_FUNCS( [getmouse] )
AC_DEFINE([NCURSES_ENABLE_STDBOOL_H], [1], [Define to enable stdbool.h in ncurses])


AC_ARG_ENABLE([affinity],
              [AS_HELP_STRING([--enable-affinity],
                              [enable sched_setaffinity and sched_getaffinity for affinity support, conflicts with hwloc @<:@default=check@:>@])],
              [],
              [enable_affinity=check])
if test "x$enable_affinity" = xcheck; then
   if test "x$enable_hwloc" = xyes; then
      enable_affinity=no
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
      [enable_affinity=yes
         AC_MSG_RESULT([yes])],
      [enable_affinity=no
         AC_MSG_RESULT([no])],
      [AC_MSG_RESULT([assume yes (cross compiling)])])
   fi
fi
if test "x$enable_affinity" = xyes; then
   if test "x$enable_hwloc" = xyes; then
      AC_MSG_ERROR([--enable-hwloc and --enable-affinity are mutual exclusive. Specify at most one of them.])
   fi
   AC_DEFINE([HAVE_AFFINITY], [1], [Define if sched_setaffinity and sched_getaffinity are to be used.])
fi


AC_ARG_ENABLE([unwind],
              [AS_HELP_STRING([--enable-unwind],
              [enable unwind support for printing backtraces; requires libunwind @<:@default=check@:>@])],
              [],
              [enable_unwind=check])
case "$enable_unwind" in
   check)
      enable_unwind=yes
      if test "$enable_static" = yes; then
         AC_CHECK_LIB([lzma], [lzma_index_buffer_decode])
      fi
      AC_CHECK_LIB([unwind], [backtrace], [], [enable_unwind=no])
      AC_CHECK_HEADERS([libunwind.h], [], [
         old_CFLAGS="$CFLAGS"
         CFLAGS="$CFLAGS -I/usr/include/libunwind"
         AC_CHECK_HEADERS([libunwind/libunwind.h], [], [
            enable_unwind=no
            CFLAGS="$old_CFLAGS"
         ])
      ])
      ;;
   no)
      ;;
   yes)
      AC_CHECK_LIB([unwind], [backtrace], [], [AC_MSG_ERROR([can not find required library libunwind])])
      AC_CHECK_HEADERS([libunwind.h], [], [
         AM_CFLAGS="$AM_CFLAGS -I/usr/include/libunwind"
         AC_CHECK_HEADERS([libunwind/libunwind.h], [], [AC_MSG_ERROR([can not find required header file libunwind.h])])
      ])
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_unwind' for --enable-unwind])
      ;;
esac
if test "x$enable_unwind" = xno; then
   # Fall back to backtrace(3) and add -lexecinfo if needed
   AC_SEARCH_LIBS([backtrace], [execinfo])

   AC_MSG_CHECKING([the return type of backtrace])
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#ifndef BACKTRACE_RETURN_TYPE
#error "BACKTRACE_RETURN_TYPE not defined"
#endif
      ]])],
      [AC_MSG_RESULT([user defined])],
      [htop_save_CFLAGS=$CFLAGS
      CFLAGS="$AM_CFLAGS $CFLAGS -Werror"

      # Use 'typeof' if supported. Otherwise guess types from larger bit width
      # to smaller.
      # 'size_t' is used in FreeBSD, NetBSD and OpenBSD.
      # 'int' is used in Linux (with glibc) and macOS.
      # See also: Gnulib documentation of 'backtrace'
      # (https://www.gnu.org/software/gnulib/manual/html_node/backtrace.html).
      # See https://github.com/htop-dev/htop/issues/1681 and
      # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119893 for the gcc bug
      # why we do 1 and not 0 below.
      for backtrace_return_type in \
         "typeof(backtrace((void**)1, 1))" \
         "__typeof__(backtrace((void**)1, 1))" \
         "__typeof(backtrace((void**)1, 1))" \
         size_t \
         int \
      ; do
         AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <execinfo.h>
#include <stddef.h> /* For size_t */
         ]],[[
#define BACKTRACE_RETURN_TYPE $backtrace_return_type
/* Assuming the second argument of backtrace_symbols() would use the same type
   as the return type of backtrace(). */
static char** (*func)(void* const*, BACKTRACE_RETURN_TYPE) = backtrace_symbols;
static void* addr;
BACKTRACE_RETURN_TYPE nptrs = backtrace(&addr, 1);
(void)nptrs;
         ]])],
         [AC_DEFINE_UNQUOTED([BACKTRACE_RETURN_TYPE], [$backtrace_return_type], [Defined to the return type of the 'backtrace' function.])
         break],
         [backtrace_return_type=""])
      done
      backtrace_return_type_msg=`echo "x$backtrace_return_type" | sed '
         s/^x *//
         s/^$/not available/
         s/^\(_*typeof_*\) *( *backtrace( *( *void *\* *\* *) *0 *, *0 *) *) *$/automatic (using \1)/'`
      AC_MSG_RESULT([$backtrace_return_type_msg])

      CFLAGS=$htop_save_CFLAGS])
fi


AC_ARG_ENABLE([hwloc],
              [AS_HELP_STRING([--enable-hwloc],
              [enable hwloc support for CPU affinity; disables affinity support; requires libhwloc @<:@default=no@:>@])],
              [],
              [enable_hwloc=no])
case "$enable_hwloc" in
   no)
      ;;
   yes)
      m4_ifdef([PKG_PROG_PKG_CONFIG], [
         PKG_CHECK_MODULES(HWLOC, hwloc, [
               AM_CFLAGS="$AM_CFLAGS $HWLOC_CFLAGS"
               LIBS="$LIBS $HWLOC_LIBS"
               AC_DEFINE([HAVE_LIBHWLOC], [1], [Define to 1 if you have the 'hwloc' library (-lhwloc).])
            ], [
               AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [AC_MSG_ERROR([can not find required library libhwloc])])
               AC_CHECK_HEADERS([hwloc.h], [], [AC_MSG_ERROR([can not find require header file hwloc.h])])
            ])
         ], [
            AC_CHECK_LIB([hwloc], [hwloc_get_proc_cpubind], [], [AC_MSG_ERROR([can not find required library libhwloc])])
            AC_CHECK_HEADERS([hwloc.h], [], [AC_MSG_ERROR([can not find require header file hwloc.h])])
         ])
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

AC_ARG_WITH([config],
            [AS_HELP_STRING([--with-config=DIR],
                            [configuration path @<:@default=/.config@:>@])],
            [],
            [with_config="/.config"])
dnl Performance Co-Pilot configuration location to prevent overwrite
if test "$my_htop_platform" = pcp -a "$with_config" = /.config; then
    with_config="/.pcp"
elif test -z "$with_config"; then
   AC_MSG_ERROR([bad empty value for --with-config option])
fi
AC_DEFINE_UNQUOTED([CONFIGDIR], ["$with_config"], [Configuration path.])

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
   if test "x$enable_vserver" != xyes; then
      enable_vserver=implied
   fi
   AC_DEFINE([HAVE_VSERVER], [1], [Define if VServer support enabled.])
   AC_DEFINE([HAVE_ANCIENT_VSERVER], [1], [Define if ancient vserver support enabled.])
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
         old_CFLAGS="$CFLAGS"
         CFLAGS="$CFLAGS -I/usr/include/libnl3"
         AC_CHECK_HEADERS([netlink/attr.h netlink/handlers.h netlink/msg.h], [enable_delayacct=yes], [enable_delayacct=no])
         CFLAGS="$old_CFLAGS"
      fi
      ;;
   yes)
      old_CFLAGS="$CFLAGS"
      CFLAGS="$CFLAGS -I/usr/include/libnl3"
      AC_CHECK_HEADERS([netlink/attr.h netlink/handlers.h netlink/msg.h], [], [AC_MSG_ERROR([can not find required header files netlink/attr.h, netlink/handlers.h, netlink/msg.h])])
      CFLAGS="$old_CFLAGS"
      ;;
   *)
      AC_MSG_ERROR([bad value '$enable_delayacct' for --enable-delayacct])
      ;;
esac
if test "$enable_delayacct" = yes; then
  AC_DEFINE([HAVE_DELAYACCT], [1], [Define if delay accounting support should be enabled.])
  AM_CFLAGS="$AM_CFLAGS -I/usr/include/libnl3"
fi
AM_CONDITIONAL([HAVE_DELAYACCT], [test "$enable_delayacct" = yes])


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

AM_CFLAGS="$AM_CFLAGS\
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

AX_CHECK_COMPILE_FLAG([-Wextra-semi-stmt],         [AM_CFLAGS="$AM_CFLAGS -Wextra-semi-stmt"],         , [-Werror=unknown-warning-option]) dnl the autoconf check itself generates -Wextra-semi-stmt
AX_CHECK_COMPILE_FLAG([-Wimplicit-int-conversion], [AM_CFLAGS="$AM_CFLAGS -Wimplicit-int-conversion"], , [-Werror])
AX_CHECK_COMPILE_FLAG([-Wnull-dereference],        [AM_CFLAGS="$AM_CFLAGS -Wnull-dereference"],        , [-Werror])

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

   AC_COMPILE_IFELSE([
      AC_LANG_SOURCE([[
#ifdef __SUPPORT_SNAN__
#error "signaling NaN support not recommended"
#endif
      ]])],
      :,
      [warning_msg="signaling NaN support is enabled; not recommended for htop"
      case "$CC" in
      *gcc*)
         warning_msg="$warning_msg (use '-fno-signaling-nans' compiler flag to disable)"
         ;;
      esac
      AC_MSG_WARN([$warning_msg])
      ])
else
   AM_CPPFLAGS="$AM_CPPFLAGS -ggdb3"
fi


AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])

# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# We're done, let's go!
# ----------------------------------------------------------------------

AC_DEFINE_UNQUOTED([COPYRIGHT], ["(C) 2004-2019 Hisham Muhammad. (C) 2020-2025 htop dev team."], [Copyright message.])

AM_CONDITIONAL([HTOP_LINUX], [test "$my_htop_platform" = linux])
AM_CONDITIONAL([HTOP_FREEBSD], [test "$my_htop_platform" = freebsd])
AM_CONDITIONAL([HTOP_DRAGONFLYBSD], [test "$my_htop_platform" = dragonflybsd])
AM_CONDITIONAL([HTOP_NETBSD], [test "$my_htop_platform" = netbsd])
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

  platform:                  $my_htop_platform ($host_os)
  os-release file:           $with_os_release
  (Linux) proc directory:    $with_proc
  (Linux) openvz:            $enable_openvz
  (Linux) vserver:           $enable_vserver
  (Linux) ancient vserver:   $enable_ancient_vserver
  (Linux) delay accounting:  $enable_delayacct
  (Linux) sensors:           $enable_sensors
  (Linux) capabilities:      $enable_capabilities
  unicode:                   $enable_unicode
  affinity:                  $enable_affinity
  unwind:                    $enable_unwind
  hwloc:                     $enable_hwloc
  debug:                     $enable_debug
  static:                    $enable_static
])
