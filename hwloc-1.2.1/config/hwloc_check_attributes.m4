# This macro set originally copied from Open MPI:
# Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2007 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# and renamed for hwloc:
# Copyright (c) 2009 INRIA.  All rights reserved.
# Copyright (c) 2009 Université Bordeaux 1
# Copyright (c) 2010 Cisco Systems, Inc.  All rights reserved.
# See COPYING in top-level directory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# 
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer listed
#   in this license in the documentation and/or other materials
#   provided with the distribution.
# 
# - Neither the name of the copyright holders nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
# 
# The copyright holders provide no reassurances that the source code
# provided does not infringe any patent, copyright, or any other
# intellectual property rights of third parties.  The copyright holders
# disclaim any liability to any recipient for claims brought against
# recipient by any third party for infringement of that parties
# intellectual property rights.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# Search the generated warnings for 
# keywords regarding skipping or ignoring certain attributes
#   Intel: ignore
#   Sun C++: skip
#
AC_DEFUN([_HWLOC_ATTRIBUTE_FAIL_SEARCH],[
    # Be safe for systems that have ancient Autoconf's (e.g., RHEL5)
    m4_ifdef([AC_PROG_GREP], 
             [AC_REQUIRE([AC_PROG_GREP])],
             [GREP=grep])

    if test -s conftest.err ; then
        for i in ignore skip ; do
            $GREP -iq $i conftest.err
            if test "$?" = "0" ; then
                hwloc_cv___attribute__[$1]=0
                break;
            fi
        done
    fi
])

#
# HWLOC: Remove C++ compiler check.  It can result in a circular
# dependency in embedded situations.
#
# Check for one specific attribute by compiling with C
# and possibly using a cross-check.
#
# If the cross-check is defined, a static function "usage" should be
# defined, which is to be called from main (to circumvent warnings
# regarding unused function in main file)
#       static int usage (int * argument);
#
# The last argument is for specific CFLAGS, that need to be set 
# for the compiler to generate a warning on the cross-check.
# This may need adaption for future compilers / CFLAG-settings.
#
AC_DEFUN([_HWLOC_CHECK_SPECIFIC_ATTRIBUTE], [
    AC_MSG_CHECKING([for __attribute__([$1])])
    AC_CACHE_VAL(hwloc_cv___attribute__[$1], [
        #
        # Try to compile using the C compiler
        #
        AC_TRY_COMPILE([$2],[],
                       [
                        #
                        # In case we did succeed: Fine, but was this due to the
                        # attribute being ignored/skipped? Grep for IgNoRe/skip in conftest.err
                        # and if found, reset the hwloc_cv__attribute__var=0
                        #
                        hwloc_cv___attribute__[$1]=1
                        _HWLOC_ATTRIBUTE_FAIL_SEARCH([$1])
                       ],
                       [hwloc_cv___attribute__[$1]=0])
        
        #
        # If the attribute is supported by both compilers,
        # try to recompile a *cross-check*, IFF defined.
        #
        if test '(' "$hwloc_cv___attribute__[$1]" = "1" -a "[$3]" != "" ')' ; then
            ac_c_werror_flag_safe=$ac_c_werror_flag
            ac_c_werror_flag="yes"
            CFLAGS_safe=$CFLAGS
            CFLAGS="$CFLAGS [$4]"

            AC_TRY_COMPILE([$3],
                [
                 int i=4711;
                 i=usage(&i);
                ],
                [hwloc_cv___attribute__[$1]=0],
                [
                 #
                 # In case we did NOT succeed: Fine, but was this due to the
                 # attribute being ignored? Grep for IgNoRe in conftest.err
                 # and if found, reset the hwloc_cv__attribute__var=0
                 #
                 hwloc_cv___attribute__[$1]=1
                 _HWLOC_ATTRIBUTE_FAIL_SEARCH([$1])
                ])

            ac_c_werror_flag=$ac_c_werror_flag_safe
            CFLAGS=$CFLAGS_safe
        fi
    ])

    if test "$hwloc_cv___attribute__[$1]" = "1" ; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
    fi
])


#
# Test the availability of __attribute__ and with the help
# of _HWLOC_CHECK_SPECIFIC_ATTRIBUTE for the support of
# particular attributes. Compilers, that do not support an
# attribute most often fail with a warning (when the warning
# level is set).
# The compilers output is parsed in _HWLOC_ATTRIBUTE_FAIL_SEARCH
# 
# To add a new attributes __NAME__ add the
#   hwloc_cv___attribute__NAME
# add a new check with _HWLOC_CHECK_SPECIFIC_ATTRIBUTE (possibly with a cross-check)
#   _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([name], [int foo (int arg) __attribute__ ((__name__));], [], [])
# and define the corresponding
#   AC_DEFINE_UNQUOTED(_HWLOC_HAVE_ATTRIBUTE_NAME, [$hwloc_cv___attribute__NAME],
#                      [Whether your compiler has __attribute__ NAME or not])
# and decide on a correct macro (in opal/include/opal_config_bottom.h):
#  #  define __opal_attribute_NAME(x)  __attribute__(__NAME__)
#
# Please use the "__"-notation of the attribute in order not to
# clash with predefined names or macros (e.g. const, which some compilers
# do not like..)
#


AC_DEFUN([_HWLOC_CHECK_ATTRIBUTES], [
  AC_MSG_CHECKING(for __attribute__)

  AC_CACHE_VAL(hwloc_cv___attribute__, [
    AC_TRY_COMPILE(
      [#include <stdlib.h>
       /* Check for the longest available __attribute__ (since gcc-2.3) */
       struct foo {
           char a;
           int x[2] __attribute__ ((__packed__));
        };
      ],
      [],
      [hwloc_cv___attribute__=1],
      [hwloc_cv___attribute__=0],
    )

    if test "$hwloc_cv___attribute__" = "1" ; then
        AC_TRY_COMPILE(
          [#include <stdlib.h>
           /* Check for the longest available __attribute__ (since gcc-2.3) */
           struct foo {
               char a;
               int x[2] __attribute__ ((__packed__));
            };
          ],
          [],
          [hwloc_cv___attribute__=1],
          [hwloc_cv___attribute__=0],
        )
    fi
    ])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE, [$hwloc_cv___attribute__],
                     [Whether your compiler has __attribute__ or not])

#
# Now that we know the compiler support __attribute__ let's check which kind of
# attributed are supported.
#
  if test "$hwloc_cv___attribute__" = "0" ; then
    AC_MSG_RESULT([no])
    hwloc_cv___attribute__aligned=0
    hwloc_cv___attribute__always_inline=0
    hwloc_cv___attribute__cold=0
    hwloc_cv___attribute__const=0
    hwloc_cv___attribute__deprecated=0
    hwloc_cv___attribute__format=0
    hwloc_cv___attribute__hot=0
    hwloc_cv___attribute__malloc=0
    hwloc_cv___attribute__may_alias=0
    hwloc_cv___attribute__no_instrument_function=0
    hwloc_cv___attribute__nonnull=0
    hwloc_cv___attribute__noreturn=0
    hwloc_cv___attribute__packed=0
    hwloc_cv___attribute__pure=0
    hwloc_cv___attribute__sentinel=0
    hwloc_cv___attribute__unused=0
    hwloc_cv___attribute__warn_unused_result=0
    hwloc_cv___attribute__weak_alias=0
  else
    AC_MSG_RESULT([yes])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([aligned],
        [struct foo { char text[4]; }  __attribute__ ((__aligned__(8)));],
        [],
        [])

    #
    # Ignored by PGI-6.2.5; -- recognized by output-parser
    #
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([always_inline],
        [int foo (int arg) __attribute__ ((__always_inline__));],
        [],
        [])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([cold],
        [
         int foo(int arg1, int arg2) __attribute__ ((__cold__));
         int foo(int arg1, int arg2) { return arg1 * arg2 + arg1; }
        ],
        [],
        [])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([const],
        [
         int foo(int arg1, int arg2) __attribute__ ((__const__));
         int foo(int arg1, int arg2) { return arg1 * arg2 + arg1; }
        ],
        [],
        [])


    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([deprecated],
        [
         int foo(int arg1, int arg2) __attribute__ ((__deprecated__));
         int foo(int arg1, int arg2) { return arg1 * arg2 + arg1; }
        ],
        [],
        [])


    HWLOC_ATTRIBUTE_CFLAGS=
    case "$hwloc_c_vendor" in
        gnu)
            HWLOC_ATTRIBUTE_CFLAGS="-Wall"
            ;;
        intel)
            # we want specifically the warning on format string conversion
            HWLOC_ATTRIBUTE_CFLAGS="-we181"
            ;;
    esac
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([format],
        [
         int this_printf (void *my_object, const char *my_format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
        ],
        [
         static int usage (int * argument);
         extern int this_printf (int arg1, const char *my_format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));

         static int usage (int * argument) {
             return this_printf (*argument, "%d", argument); /* This should produce a format warning */
         }
         /* The autoconf-generated main-function is int main(), which produces a warning by itself */
         int main(void);
        ],
        [$HWLOC_ATTRIBUTE_CFLAGS])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([hot],
        [
         int foo(int arg1, int arg2) __attribute__ ((__hot__));
         int foo(int arg1, int arg2) { return arg1 * arg2 + arg1; }
        ],
        [],
        [])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([malloc],
        [
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
         int * foo(int arg1) __attribute__ ((__malloc__));
         int * foo(int arg1) { return (int*) malloc(arg1); }
        ],
        [],
        [])


    #
    # Attribute may_alias: No suitable cross-check available, that works for non-supporting compilers
    # Ignored by intel-9.1.045 -- turn off with -wd1292
    # Ignored by PGI-6.2.5; ignore not detected due to missing cross-check
    #
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([may_alias],
        [int * p_value __attribute__ ((__may_alias__));],
        [],
        [])


    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([no_instrument_function],
        [int * foo(int arg1) __attribute__ ((__no_instrument_function__));],
        [],
        [])


    #
    # Attribute nonnull:
    # Ignored by intel-compiler 9.1.045 -- recognized by cross-check
    # Ignored by PGI-6.2.5 (pgCC) -- recognized by cross-check
    #
    HWLOC_ATTRIBUTE_CFLAGS=
    case "$hwloc_c_vendor" in
        gnu)
            HWLOC_ATTRIBUTE_CFLAGS="-Wall"
            ;;
        intel)
            # we do not want to get ignored attributes warnings, but rather real warnings
            HWLOC_ATTRIBUTE_CFLAGS="-wd1292"
            ;;
    esac
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([nonnull],
        [
         int square(int *arg) __attribute__ ((__nonnull__));
         int square(int *arg) { return *arg; }
        ],
        [
         static int usage(int * argument);
         int square(int * argument) __attribute__ ((__nonnull__));
         int square(int * argument) { return (*argument) * (*argument); }

         static int usage(int * argument) {
             return square( ((void*)0) );    /* This should produce an argument must be nonnull warning */
         }
         /* The autoconf-generated main-function is int main(), which produces a warning by itself */
         int main(void);
        ],
        [$HWLOC_ATTRIBUTE_CFLAGS])


    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([noreturn],
        [
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
         void fatal(int arg1) __attribute__ ((__noreturn__));
         void fatal(int arg1) { exit(arg1); }
        ],
        [],
        [])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([packed],
        [
         struct foo {
             char a;
             int x[2] __attribute__ ((__packed__));
         };
        ],
        [],
        [])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([pure],
        [
         int square(int arg) __attribute__ ((__pure__));
         int square(int arg) { return arg * arg; }
        ],
        [],
        [])

    #
    # Attribute sentinel:
    # Ignored by the intel-9.1.045 -- recognized by cross-check
    #                intel-10.0beta works fine
    # Ignored by PGI-6.2.5 (pgCC) -- recognized by output-parser and cross-check
    # Ignored by pathcc-2.2.1 -- recognized by cross-check (through grep ignore)
    #
    HWLOC_ATTRIBUTE_CFLAGS=
    case "$hwloc_c_vendor" in
        gnu)
            HWLOC_ATTRIBUTE_CFLAGS="-Wall"
            ;;
        intel)
            # we do not want to get ignored attributes warnings
            HWLOC_ATTRIBUTE_CFLAGS="-wd1292"
            ;;
    esac
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([sentinel],
        [
         int my_execlp(const char * file, const char *arg, ...) __attribute__ ((__sentinel__));
        ],
        [
         static int usage(int * argument);
         int my_execlp(const char * file, const char *arg, ...) __attribute__ ((__sentinel__));

         static int usage(int * argument) {
             void * last_arg_should_be_null = argument;
             return my_execlp ("lala", "/home/there", last_arg_should_be_null);   /* This should produce a warning */
         }
         /* The autoconf-generated main-function is int main(), which produces a warning by itself */
         int main(void);
        ],
        [$HWLOC_ATTRIBUTE_CFLAGS])

    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([unused],
        [
         int square(int arg1 __attribute__ ((__unused__)), int arg2);
         int square(int arg1, int arg2) { return arg2; }
        ],
        [],
        [])


    #
    # Attribute warn_unused_result:
    # Ignored by the intel-compiler 9.1.045 -- recognized by cross-check
    # Ignored by pathcc-2.2.1 -- recognized by cross-check (through grep ignore)
    #
    HWLOC_ATTRIBUTE_CFLAGS=
    case "$hwloc_c_vendor" in
        gnu)
            HWLOC_ATTRIBUTE_CFLAGS="-Wall"
            ;;
        intel)
            # we do not want to get ignored attributes warnings
            HWLOC_ATTRIBUTE_CFLAGS="-wd1292"
            ;;
    esac
    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([warn_unused_result],
        [
         int foo(int arg) __attribute__ ((__warn_unused_result__));
         int foo(int arg) { return arg + 3; }
        ],
        [
         static int usage(int * argument);
         int foo(int arg) __attribute__ ((__warn_unused_result__));

         int foo(int arg) { return arg + 3; }
         static int usage(int * argument) {
           foo (*argument);        /* Should produce an unused result warning */
           return 0;
         }

         /* The autoconf-generated main-function is int main(), which produces a warning by itself */
         int main(void);
        ],
        [$HWLOC_ATTRIBUTE_CFLAGS])


    _HWLOC_CHECK_SPECIFIC_ATTRIBUTE([weak_alias],
        [
         int foo(int arg);
         int foo(int arg) { return arg + 3; }
         int foo2(int arg) __attribute__ ((__weak__, __alias__("foo")));
        ],
        [],
        [])

  fi

  # Now that all the values are set, define them

  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_ALIGNED, [$hwloc_cv___attribute__aligned],
                     [Whether your compiler has __attribute__ aligned or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_ALWAYS_INLINE, [$hwloc_cv___attribute__always_inline],
                     [Whether your compiler has __attribute__ always_inline or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_COLD, [$hwloc_cv___attribute__cold],
                     [Whether your compiler has __attribute__ cold or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_CONST, [$hwloc_cv___attribute__const],
                     [Whether your compiler has __attribute__ const or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_DEPRECATED, [$hwloc_cv___attribute__deprecated],
                     [Whether your compiler has __attribute__ deprecated or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_FORMAT, [$hwloc_cv___attribute__format],
                     [Whether your compiler has __attribute__ format or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_HOT, [$hwloc_cv___attribute__hot],
                     [Whether your compiler has __attribute__ hot or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_MALLOC, [$hwloc_cv___attribute__malloc],
                     [Whether your compiler has __attribute__ malloc or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_MAY_ALIAS, [$hwloc_cv___attribute__may_alias],
                     [Whether your compiler has __attribute__ may_alias or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_NO_INSTRUMENT_FUNCTION, [$hwloc_cv___attribute__no_instrument_function],
                     [Whether your compiler has __attribute__ no_instrument_function or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_NONNULL, [$hwloc_cv___attribute__nonnull],
                     [Whether your compiler has __attribute__ nonnull or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_NORETURN, [$hwloc_cv___attribute__noreturn],
                     [Whether your compiler has __attribute__ noreturn or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_PACKED, [$hwloc_cv___attribute__packed],
                     [Whether your compiler has __attribute__ packed or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_PURE, [$hwloc_cv___attribute__pure],
                     [Whether your compiler has __attribute__ pure or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_SENTINEL, [$hwloc_cv___attribute__sentinel],
                     [Whether your compiler has __attribute__ sentinel or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_UNUSED, [$hwloc_cv___attribute__unused],
                     [Whether your compiler has __attribute__ unused or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_WARN_UNUSED_RESULT, [$hwloc_cv___attribute__warn_unused_result],
                     [Whether your compiler has __attribute__ warn unused result or not])
  AC_DEFINE_UNQUOTED(HWLOC_HAVE_ATTRIBUTE_WEAK_ALIAS, [$hwloc_cv___attribute__weak_alias],
                     [Whether your compiler has __attribute__ weak alias or not])
])

