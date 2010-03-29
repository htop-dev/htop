# -*- shell-script -*-
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2004-2008 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2006-2009 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# Main PLPA m4 macro, to be invoked by the user
#
# Expects two or three paramters:
# 1. Configuration prefix (optional; if not specified, "." is assumed)
# 2. What to do upon success
# 3. What to do upon failure
#
AC_DEFUN([PLPA_INIT],[
    # If we used the 2 param variant of PLPA_INIT, then assume the
    # config prefix is ".".  Otherwise, it's $1.
    m4_ifval([$3], 
             [_PLPA_INIT_COMPAT([$1], [$2], [$3])],
             [AC_MSG_WARN([The 2-argument form of the PLPA INIT m4 macro is deprecated])
              AC_MSG_WARN([It was removed starting with PLPA v1.2])
              AC_MSG_ERROR([Cannot continue])])
])dnl

#-----------------------------------------------------------------------

# Do the main work for PLPA_INIT
#
# Expects three paramters:
# 1. Configuration prefix
# 2. What to do upon success
# 3. What to do upon failure
#
AC_DEFUN([_PLPA_INIT_COMPAT],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    AC_REQUIRE([AC_PROG_CC])
    AC_REQUIRE([AM_PROG_LEX])
    AC_REQUIRE([AC_PROG_YACC])

    m4_define([plpa_config_prefix],[$1])

    # Check for syscall()
    AC_CHECK_FUNC([syscall], [plpa_config_happy=1], [plpa_config_happy=0])

    # Look for syscall.h
    if test "$plpa_config_happy" = 1; then
        AC_CHECK_HEADER([sys/syscall.h], [plpa_config_happy=1], [plpa_config_happy=0])
    fi

    # Look for unistd.h
    if test "$plpa_config_happy" = 1; then
        AC_CHECK_HEADER([unistd.h], [plpa_config_happy=1], [plpa_config_happy=0])
    fi

    # Check for __NR_sched_setaffinity
    if test "$plpa_config_happy" = 1; then
        AC_MSG_CHECKING([for __NR_sched_setaffinity])
        if test "$plpa_emulate" = "yes"; then
            AC_MSG_RESULT([emulated])
            AC_DEFINE([__NR_sched_setaffinity], [0], [Emulated value])
        else
            AC_TRY_COMPILE([#include <syscall.h>
#include <unistd.h>], [#ifndef __NR_sched_setaffinity
#error __NR_sched_setaffinity_not found!
#endif
int i = 1;],
                           [AC_MSG_RESULT([yes])
                            plpa_config_happy=1], 
                           [AC_MSG_RESULT([no])
                            plpa_config_happy=0])
        fi
    fi

    # Check for __NR_sched_getaffinity (probably overkill, but what
    # the heck?)
    if test "$plpa_config_happy" = 1; then
        AC_MSG_CHECKING([for __NR_sched_getaffinity])
        if test "$plpa_emulate" = "yes"; then
            AC_MSG_RESULT([emulated])
            AC_DEFINE([__NR_sched_getaffinity], [0], [Emulated value])
        else
            AC_TRY_COMPILE([#include <syscall.h>
#include <unistd.h>], [#ifndef __NR_sched_getaffinity
#error __NR_sched_getaffinity_not found!
#endif
int i = 1;],
                           [AC_MSG_RESULT([yes])
                            plpa_config_happy=1], 
                           [AC_MSG_RESULT([no])
                            plpa_config_happy=0])
        fi
    fi

    # If all was good, do the real init
    AS_IF([test "$plpa_config_happy" = "1"],
          [_PLPA_INIT($2, $3)],
          [$3])
    PLPA_DO_AM_CONDITIONALS

    AC_CONFIG_FILES(
        plpa_config_prefix[/Makefile]
        plpa_config_prefix[/src/Makefile]
        plpa_config_prefix[/src/libplpa/Makefile]
    )

    # Cleanup
    unset plpa_config_happy
])dnl

#-----------------------------------------------------------------------

# Build PLPA as a standalone package
AC_DEFUN([PLPA_STANDALONE],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    plpa_mode=standalone
])dnl

#-----------------------------------------------------------------------

# Build PLPA as an included package
AC_DEFUN([PLPA_INCLUDED],[
    m4_ifval([$1], 
             [AC_MSG_WARN([The 1-argument form of the PLPA INCLUDED m4 macro is deprecated])
              AC_MSG_WARN([It was removed starting with PLPA v1.2])
              AC_MSG_ERROR([Cannot continue])])

    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    plpa_mode=included
    PLPA_DISABLE_EXECUTABLES
])dnl

#-----------------------------------------------------------------------

dnl JMS: No fortran bindings yet
dnl # Set whether the fortran bindings will be built or not
dnl AC_DEFUN([PLPA_FORTRAN],[
dnl     AC_REQUIRE([_PLPA_INTERNAL_SETUP])
dnl 
dnl    # Need [] around entire following line to escape m4 properly
dnl     [plpa_tmp=`echo $1 | tr '[:upper:]' '[:lower:]'`]
dnl     if test "$1" = "0" -o "$1" = "n"; then
dnl         plpa_fortran=no
dnl     elif test "$1" = "1" -o "$1" = "y"; then
dnl         plpa_fortran=yes
dnl     else
dnl         AC_MSG_WARN([Did not understand PLPA_FORTRAN argument ($1) -- ignored])
dnl     fi
dnl ])dnl

#-----------------------------------------------------------------------

# Disable building the executables
AC_DEFUN([PLPA_DISABLE_EXECUTABLES],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    plpa_executables=no
])dnl

#-----------------------------------------------------------------------

# Disable building the executables
AC_DEFUN([PLPA_ENABLE_EXECUTABLES],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    plpa_executables=yes
])dnl

#-----------------------------------------------------------------------

# Specify the symbol prefix
AC_DEFUN([PLPA_SET_SYMBOL_PREFIX],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])
    plpa_symbol_prefix_value=$1
])dnl

#-----------------------------------------------------------------------

# Internals
AC_DEFUN([_PLPA_INTERNAL_SETUP],[

    AC_ARG_ENABLE([emulate],
                    AC_HELP_STRING([--enable-emulate],
                                   [Emulate __NR_sched_setaffinity and __NR_sched_getaffinity, to allow building on non-Linux systems (for testing)]))
    if test "$enable_emulate" = "yes"; then
        plpa_emulate=yes
    else
        plpa_emulate=no
    fi

    # Build and install the executables or no?
    AC_ARG_ENABLE([executables],
                    AC_HELP_STRING([--disable-executables],
                                   [Using --disable-executables disables building and installing the PLPA executables]))
    if test "$enable_executables" = "yes" -o "$enable_executables" = ""; then
        plpa_executables=yes
    else
        plpa_executables=no
    fi

    # Included mode, or standalone?
    AC_ARG_ENABLE([included-mode],
                    AC_HELP_STRING([--enable-included-mode],
                                   [Using --enable-included-mode puts the PLPA into "included" mode.  The default is --disable-included-mode, meaning that the PLPA is in "standalone" mode.]))
    if test "$enable_included_mode" = "yes"; then
        plpa_mode=included
        if test "$enable_executables" = ""; then
            plpa_executables=no
        fi
    else
        plpa_mode=standalone
    fi

dnl JMS: No fortran bindings yet
dnl    # Fortran bindings, or no?
dnl    AC_ARG_ENABLE([fortran],
dnl                    AC_HELP_STRING([--disable-fortran],
dnl                                   [Using --disable-fortran disables building the Fortran PLPA API bindings]))
dnl    if test "$enable_fortran" = "yes" -o "$enable_fortran" = ""; then
dnl        plpa_fortran=yes
dnl    else
dnl        plpa_fortran=no
dnl    fi

    # Change the symbol prefix?
    AC_ARG_WITH([plpa-symbol-prefix],
                AC_HELP_STRING([--with-plpa-symbol-prefix=STRING],
                               [STRING can be any valid C symbol name.  It will be prefixed to all public PLPA symbols.  Default: "plpa_"]))
    if test "$with_plpa_symbol_prefix" = ""; then
        plpa_symbol_prefix_value=plpa_
    else
        plpa_symbol_prefix_value=$with_plpa_symbol_prefix
    fi

    # Debug mode?
    AC_ARG_ENABLE([debug],
                    AC_HELP_STRING([--enable-debug],
                                   [Using --enable-debug enables various maintainer-level debugging controls.  This option is not recomended for end users.]))
    if test "$enable_debug" = "yes"; then
        plpa_debug=1
        plpa_debug_msg="enabled"
    elif test "$enable_debug" = "" -a -d .svn; then
        plpa_debug=1
        plpa_debug_msg="enabled (SVN checkout default)"
    else
        plpa_debug=0
        plpa_debug_msg="disabled"
    fi
])dnl

#-----------------------------------------------------------------------

# Internals for PLPA_INIT
AC_DEFUN([_PLPA_INIT],[
    AC_REQUIRE([_PLPA_INTERNAL_SETUP])

    # Are we building as standalone or included?
    AC_MSG_CHECKING([for PLPA building mode])
    AC_MSG_RESULT([$plpa_mode])

    # Debug mode?
    AC_MSG_CHECKING([if want PLPA maintainer support])
    AC_DEFINE_UNQUOTED(PLPA_DEBUG, [$plpa_debug], [Whether we are in debugging more or not])
    AC_MSG_RESULT([$plpa_debug_msg])

    # We need to set a path for header, etc files depending on whether
    # we're standalone or included. this is taken care of by PLPA_INCLUDED.

    AC_MSG_CHECKING([for PLPA config prefix])
    AC_MSG_RESULT(plpa_config_prefix)

    # Note that plpa_config.h *MUST* be listed first so that it
    # becomes the "main" config header file.  Any AM_CONFIG_HEADERs
    # after that (plpa.h) will only have selective #defines replaced,
    # not the entire file.
    AM_CONFIG_HEADER(plpa_config_prefix[/src/libplpa/plpa_config.h])
    AM_CONFIG_HEADER(plpa_config_prefix[/src/libplpa/plpa.h])

    # What prefix are we using?
    AC_MSG_CHECKING([for PLPA symbol prefix])
    AC_DEFINE_UNQUOTED(PLPA_SYM_PREFIX, [$plpa_symbol_prefix_value],
                       [The PLPA symbol prefix])
    # Ensure to [] escape the whole next line so that we can get the
    # proper tr tokens
    [plpa_symbol_prefix_value_caps="`echo $plpa_symbol_prefix_value | tr '[:lower:]' '[:upper:]'`"]
    AC_DEFINE_UNQUOTED(PLPA_SYM_PREFIX_CAPS, [$plpa_symbol_prefix_value_caps],
                       [The PLPA symbol prefix in all caps])
    AC_MSG_RESULT([$plpa_symbol_prefix_value])

    # Build with valgrind support if we can find it, unless it was
    # explicitly disabled
    AC_ARG_WITH([valgrind],
                [AC_HELP_STRING([--with-valgrind(=DIR)],
                [Directory where the valgrind software is installed])])
    CPPFLAGS_save="$CPPFLAGS"
    valgrind_happy=no
    AS_IF([test "$with_valgrind" != "no"],
          [AS_IF([test ! -z "$with_valgrind" -a "$with_valgrind" != "yes"],
                 [CPPFLAGS="$CPPFLAGS -I$with_valgrind/include"])
           AC_CHECK_HEADERS([valgrind/valgrind.h], 
                 [AC_MSG_CHECKING([for VALGRIND_CHECK_MEM_IS_ADDRESSABLE])
                  AC_LINK_IFELSE(AC_LANG_PROGRAM([[
#include "valgrind/memcheck.h"
]],
                     [[char buffer = 0xff;
                       VALGRIND_CHECK_MEM_IS_ADDRESSABLE(&buffer, sizeof(buffer));]]),
                     [AC_MSG_RESULT([yes])
                      valgrind_happy=yes],
                     [AC_MSG_RESULT([no])
                      AC_MSG_WARN([Need Valgrind version 3.2.0 or later.])],
                     [AC_MSG_RESULT([cross-compiling; assume yes...?])
                      AC_MSG_WARN([PLPA will fail to compile if you do not have Valgrind version 3.2.0 or later])
                      valgrind_happy=yes]),
                 ],
                 [AC_MSG_WARN([valgrind.h not found])])
           AS_IF([test "$valgrind_happy" = "no" -a "x$with_valgrind" != "x"],
                 [AC_MSG_WARN([Valgrind support requested but not possible])
                  AC_MSG_ERROR([Cannot continue])])])
    AS_IF([test "$valgrind_happy" = "no"],
          [CPPFLAGS="$CPPFLAGS_save"
           valgrind_define=0],
          [valgrind_define=1])
    AC_DEFINE_UNQUOTED([PLPA_WANT_VALGRIND_SUPPORT], [$valgrind_define],
                       [Whether we want Valgrind support or not])

dnl JMS: No fortran bindings yet
dnl    # Check for fortran
dnl    AC_MSG_CHECKING([whether to build PLPA Fortran API])
dnl    AC_MSG_RESULT([$plpa_fortran])

    # Check whether to build the exectuables or not
    AC_MSG_CHECKING([whether to build PLPA executables])
    AC_MSG_RESULT([$plpa_executables])

    # If we're building executables, we need some things for plpa-taskset
    if test "$plpa_executables" = "yes"; then
        AC_C_INLINE
    fi

    # Success
    $1
])dnl


#-----------------------------------------------------------------------

# This must be a standalone routine so that it can be called both by
# PLPA_INIT and an external caller (if PLPA_INIT is not invoked).
AC_DEFUN([PLPA_DO_AM_CONDITIONALS],[
    if test "$plpa_did_am_conditionals" != "yes"; then
        AM_CONDITIONAL([PLPA_BUILD_STANDALONE], [test "$plpa_mode" = "standalone"])
dnl JMS: No fortran bindings yet
dnl        AM_CONDITIONAL(PLPA_BUILD_FORTRAN, [test "$plpa_fortran" = "yes"])
        AM_CONDITIONAL(PLPA_BUILD_EXECUTABLES, [test "$plpa_executables" = "yes"])
    fi
    plpa_did_am_conditionals=yes
])dnl
