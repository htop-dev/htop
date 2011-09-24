dnl -*- Autoconf -*-
dnl
dnl Copyright (c) 2009 INRIA.  All rights reserved.
dnl Copyright (c) 2009, 2011 Université Bordeaux 1
dnl Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
dnl                         University Research and Technology
dnl                         Corporation.  All rights reserved.
dnl Copyright (c) 2004-2005 The Regents of the University of California.
dnl                         All rights reserved.
dnl Copyright (c) 2004-2008 High Performance Computing Center Stuttgart, 
dnl                         University of Stuttgart.  All rights reserved.
dnl Copyright ©  2010 INRIA.  All rights reserved.
dnl Copyright © 2006-2011 Cisco Systems, Inc.  All rights reserved.
dnl
dnl See COPYING in top-level directory.

#-----------------------------------------------------------------------

# Probably only ever invoked by hwloc's configure.ac
AC_DEFUN([HWLOC_BUILD_STANDALONE],[
    hwloc_mode=standalone
])dnl

#-----------------------------------------------------------------------

# Probably only ever invoked by hwloc's configure.ac
AC_DEFUN([HWLOC_DEFINE_ARGS],[
    # Embedded mode, or standalone?
    AC_ARG_ENABLE([embedded-mode],
                    AC_HELP_STRING([--enable-embedded-mode],
                                   [Using --enable-embedded-mode puts the HWLOC into "embedded" mode.  The default is --disable-embedded-mode, meaning that the HWLOC is in "standalone" mode.]))

    # Change the symbol prefix?
    AC_ARG_WITH([hwloc-symbol-prefix],
                AC_HELP_STRING([--with-hwloc-symbol-prefix=STRING],
                               [STRING can be any valid C symbol name.  It will be prefixed to all public HWLOC symbols.  Default: "hwloc_"]))

    # Debug mode?
    AC_ARG_ENABLE([debug],
                  AC_HELP_STRING([--enable-debug],
                                 [Using --enable-debug enables various hwloc maintainer-level debugging controls.  This option is not recomended for end users.]))

    # Doxygen?
    AC_ARG_ENABLE([doxygen],
        [AC_HELP_STRING([--enable-doxygen],
                        [enable support for building Doxygen documentation (note that this option is ONLY relevant in developer builds; Doxygen documentation is pre-built for tarball builds and this option is therefore ignored)])])

    # Picky?
    AC_ARG_ENABLE(picky,
                  AC_HELP_STRING([--disable-picky],
                                 [When in developer checkouts of hwloc and compiling with gcc, the default is to enable maximum compiler pickyness.  Using --disable-picky or --enable-picky overrides any default setting]))

    # Cairo?
    AC_ARG_ENABLE([cairo],
                  AS_HELP_STRING([--disable-cairo], 
                                 [Disable the Cairo back-end of hwloc's lstopo command]))

    # XML?
    AC_ARG_ENABLE([xml],
                  AS_HELP_STRING([--disable-xml], 
		                 [Disable the XML back-end of hwloc's lstopo command]))
])dnl

#-----------------------------------------------------------------------

dnl We only build documentation if this is a developer checkout.
dnl Distribution tarballs just install pre-built docuemntation that was
dnl included in the tarball.

# Probably only ever invoked by hwloc's configure.ac
AC_DEFUN([HWLOC_SETUP_DOCS],[
    cat <<EOF

###
### Configuring hwloc documentation
###
EOF

    AC_MSG_CHECKING([if this is a developer build])
    AS_IF([test ! -d "$srcdir/.svn" -a ! -d "$srcdir/.hg" -a ! -d "$srcdir/.git"],
          [AC_MSG_RESULT([no (doxygen generation is optional)])],
          [AC_MSG_RESULT([yes])])
    
    # Generating the doxygen output requires a few tools.  If we
    # don't have all of them, refuse the build the docs.
    AC_ARG_VAR([DOXYGEN], [Location of the doxygen program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([DOXYGEN], [doxygen])
    HWLOC_DOXYGEN_VERSION=`doxygen --version 2> /dev/null`
    
    AC_ARG_VAR([PDFLATEX], [Location of the pdflatex program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([PDFLATEX], [pdflatex])
    
    AC_ARG_VAR([MAKEINDEX], [Location of the makeindex program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([MAKEINDEX], [makeindex])
    
    AC_ARG_VAR([FIG2DEV], [Location of the fig2dev program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([FIG2DEV], [fig2dev])
    
    AC_ARG_VAR([GS], [Location of the gs program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([GS], [gs])

    AC_ARG_VAR([EPSTOPDF], [Location of the epstopdf program (required for building the hwloc doxygen documentation)])
    AC_PATH_TOOL([EPSTOPDF], [epstopdf])

    AC_MSG_CHECKING([if can build doxygen docs])
    AS_IF([test "x$DOXYGEN" != "x" -a "x$PDFLATEX" != "x" -a "x$MAKEINDEX" != "x" -a "x$FIG2DEV" != "x" -a "x$GS" != "x" -a "x$EPSTOPDF" != "x"],
                 [hwloc_generate_doxs=yes], [hwloc_generate_doxs=no])
    AC_MSG_RESULT([$hwloc_generate_doxs])
    
    # Linux and OS X take different sed arguments.
    AC_PROG_SED
    AC_MSG_CHECKING([if the sed -i option requires an argument])
    rm -f conftest
    cat > conftest <<EOF
hello
EOF
    $SED -i -e s/hello/goodbye/ conftest 2> /dev/null
    AS_IF([test -f conftest-e],
          [SED_I="$SED -i ''"
           AC_MSG_RESULT([yes])],
          [SED_I="$SED -i"
           AC_MSG_RESULT([no])])
    rm -f conftest conftest-e
    AC_SUBST([SED_I])

    # Making the top-level README requires w3m or lynx.
    AC_ARG_VAR([W3M], [Location of the w3m program (required to building the top-level hwloc README file)])
    AC_PATH_TOOL([W3M], [w3m])
    AC_ARG_VAR([LYNX], [Location of the lynx program (required to building the top-level hwloc README file)])
    AC_PATH_TOOL([LYNX], [lynx])
    
    AC_MSG_CHECKING([if can build top-level README])
    AS_IF([test "x$W3M" != "x"],
          [hwloc_generate_readme=yes
           HWLOC_W3_GENERATOR=$W3M],
          [AS_IF([test "x$LYNX" != "x"],
                 [hwloc_generate_readme=yes
                  HWLOC_W3_GENERATOR="$LYNX -dump -nolist"],
                 [hwloc_generate_readme=no])])
    AC_SUBST(HWLOC_W3_GENERATOR)
    AC_MSG_RESULT([$hwloc_generate_readme])
    
    # If any one of the above tools is missing, we will refuse to make dist.
    AC_MSG_CHECKING([if will build doxygen docs])
    AS_IF([test "x$hwloc_generate_doxs" = "xyes" -a "x$enable_doxygen" != "xno"],
          [], [hwloc_generate_doxs=no])
    AC_MSG_RESULT([$hwloc_generate_doxs])
    
    # See if we want to install the doxygen docs
    AC_MSG_CHECKING([if will install doxygen docs])
    AS_IF([test "x$hwloc_generate_doxs" = "xyes" -o \
    	    -f "$srcdir/doc/doxygen-doc/man/man3/hwloc_distribute.3" -a \
    	    -f "$srcdir/doc/doxygen-doc/hwloc-a4.pdf" -a \
    	    -f "$srcdir/doc/doxygen-doc/hwloc-letter.pdf"],
          [hwloc_install_doxs=yes],
          [hwloc_install_doxs=no])
    AC_MSG_RESULT([$hwloc_install_doxs])
    
    # For the common developer case, if we're in a developer checkout and
    # using the GNU compilers, turn on maximum warnings unless
    # specifically disabled by the user.
    AC_MSG_CHECKING([whether to enable "picky" compiler mode])
    hwloc_want_picky=0
    AS_IF([test "$GCC" = "yes"],
          [AS_IF([test -d "$srcdir/.svn" -o -d "$srcdir/.hg" -o -d "$srcdir/.git"],
                 [hwloc_want_picky=1])])
    if test "$enable_picky" = "yes"; then
        if test "$GCC" = "yes"; then
            AC_MSG_RESULT([yes])
            hwloc_want_picky=1
        else
            AC_MSG_RESULT([no])
            AC_MSG_WARN([Warning: --enable-picky used, but is currently only defined for the GCC compiler set -- automatically disabled])
            hwloc_want_picky=0
        fi
    elif test "$enable_picky" = "no"; then
        AC_MSG_RESULT([no])
        hwloc_want_picky=0
    else
        if test "$hwloc_want_picky" = 1; then
            AC_MSG_RESULT([yes (default)])
        else
            AC_MSG_RESULT([no (default)])
        fi
    fi
    if test "$hwloc_want_picky" = 1; then
        add="-Wall -Wunused-parameter -Wundef -Wno-long-long -Wsign-compare"
        add="$add -Wmissing-prototypes -Wstrict-prototypes"
        add="$add -Wcomment -pedantic"

        CFLAGS="$CFLAGS $add"
    fi

    # Generate some files for the docs
    AC_CONFIG_FILES(
        hwloc_config_prefix[doc/Makefile]
        hwloc_config_prefix[doc/doxygen-config.cfg])
])

#-----------------------------------------------------------------------

# Probably only ever invoked by hwloc's configure.ac
AC_DEFUN([HWLOC_SETUP_UTILS],[
    cat <<EOF

###
### Configuring hwloc command line utilities
###
EOF

    hwloc_build_utils=yes

    # Cairo support
    hwloc_cairo_happy=
    if test "x$enable_cairo" != "xno"; then
      HWLOC_PKG_CHECK_MODULES([CAIRO], [cairo], [cairo_fill],
                              [hwloc_cairo_happy=yes],
                              [hwloc_cairo_happy=no])
      if test "x$hwloc_cairo_happy" = "xyes"; then
        AC_PATH_XTRA
	CFLAGS_save=$CFLAGS
	LIBS_save=$LIBS

	CFLAGS="$CFLAGS $X_CFLAGS"
	LIBS="$LIBS $X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS"
        AC_CHECK_HEADERS([X11/Xlib.h], [
          AC_CHECK_HEADERS([X11/Xutil.h X11/keysym.h], [
            AC_CHECK_LIB([X11], [XOpenDisplay], [
              enable_X11=yes
              AC_SUBST([HWLOC_X11_LIBS], ["-lX11"])
              AC_DEFINE([HWLOC_HAVE_X11], [1], [Define to 1 if X11 libraries are available.])
            ])]
          )],,
          [[#include <X11/Xlib.h>]]
        )
        if test "x$enable_X11" != "xyes"; then
          AC_MSG_WARN([X11 headers not found, Cairo/X11 back-end disabled])
          hwloc_cairo_happy=no
        fi

	CFLAGS=$CFLAGS_save
	LIBS=$LIBS_save
      fi
    fi
    
    if test "x$hwloc_cairo_happy" = "xyes"; then
        AC_DEFINE([HWLOC_HAVE_CAIRO], [1], [Define to 1 if you have the `cairo' library.])
    else
        AS_IF([test "$enable_cairo" = "yes"],
              [AC_MSG_WARN([--enable-cairo requested, but Cairo/X11 support was not found])
               AC_MSG_ERROR([Cannot continue])])
    fi

    AC_CHECK_TYPES([wchar_t], [
      AC_CHECK_FUNCS([putwc])
    ], [], [[#include <wchar.h>]])

    AC_CHECK_HEADERS([locale.h], [
      AC_CHECK_FUNCS([setlocale])
    ])
    AC_CHECK_HEADERS([langinfo.h], [
      AC_CHECK_FUNCS([nl_langinfo])
    ])
    hwloc_old_LIBS="$LIBS"
    chosen_curses=""
    for curses in ncurses curses
    do
      for lib in "" -ltermcap -l${curses}w -l$curses
      do
        AC_MSG_CHECKING(termcap support using $curses and $lib)
        LIBS="$hwloc_old_LIBS $lib"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <$curses.h>
#include <term.h>
]], [[tparm(NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0)]])], [
          AC_MSG_RESULT(yes)
          AC_SUBST([HWLOC_TERMCAP_LIBS], ["$LIBS"])
          AC_DEFINE([HWLOC_HAVE_LIBTERMCAP], [1],
                    [Define to 1 if you have a library providing the termcap interface])
          chosen_curses=$curses
        ], [
          AC_MSG_RESULT(no)
        ])
        test "x$chosen_curses" != "x" && break
      done
      test "x$chosen_curses" != "x" && break
    done
    if test "$chosen_curses" = ncurses
    then
      AC_DEFINE([HWLOC_USE_NCURSES], [1], [Define to 1 if ncurses works, preferred over curses])
    fi
    LIBS="$hwloc_old_LIBS"
    unset hwloc_old_LIBS

    _HWLOC_CHECK_DIFF_U

    # Only generate this if we're building the utilities
    AC_CONFIG_FILES(
        hwloc_config_prefix[utils/Makefile]
        hwloc_config_prefix[hwloc.pc])
])dnl

#-----------------------------------------------------------------------

# Probably only ever invoked by hwloc's configure.ac
AC_DEFUN([HWLOC_SETUP_TESTS],[
    cat <<EOF

###
### Configuring hwloc tests
###
EOF

    hwloc_build_tests=yes

    # linux-libnuma.h testing requires libnuma with numa_bitmask_alloc()
    AC_CHECK_DECL([numa_bitmask_alloc], [hwloc_have_linux_libnuma=yes], [],
    	      [#include <numa.h>])

    AC_CHECK_HEADERS([infiniband/verbs.h], [
      AC_CHECK_LIB([ibverbs], [ibv_open_device],
                   [AC_DEFINE([HAVE_LIBIBVERBS], 1, [Define to 1 if we have -libverbs])
                    hwloc_have_libibverbs=yes])
    ])

    AC_CHECK_HEADERS([myriexpress.h], [
      AC_MSG_CHECKING(if MX_NUMA_NODE exists)
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <myriexpress.h>]],
                                         [[int a = MX_NUMA_NODE;]],
                        [AC_MSG_RESULT(yes)
                         AC_CHECK_LIB([myriexpress], [mx_get_info],
                                      [AC_DEFINE([HAVE_MYRIEXPRESS], 1, [Define to 1 if we have -lmyriexpress])
                                       hwloc_have_myriexpress=yes])],
                        [AC_MSG_RESULT(no)])])])

    AC_CHECK_HEADERS([cuda.h], [
      AC_MSG_CHECKING(if CUDA_VERSION >= 3020)
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <cuda.h>
#ifndef CUDA_VERSION
#error CUDA_VERSION undefined
#elif CUDA_VERSION < 3020
#error CUDA_VERSION too old
#endif]], [[int i = 3;]])],
       [AC_MSG_RESULT(yes)
        AC_CHECK_LIB([cuda], [cuInit],
		     [AC_DEFINE([HAVE_CUDA], 1, [Define to 1 if we have -lcuda])
		      hwloc_have_cuda=yes])],
       [AC_MSG_RESULT(no)])])

    AC_CHECK_HEADERS([cuda_runtime_api.h], [
      AC_MSG_CHECKING(if CUDART_VERSION >= 3020)
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <cuda_runtime_api.h>
#ifndef CUDART_VERSION
#error CUDART_VERSION undefined
#elif CUDART_VERSION < 3020
#error CUDART_VERSION too old
#endif]], [[int i = 3;]])],
       [AC_MSG_RESULT(yes)
        AC_CHECK_LIB([cudart], [cudaGetDeviceCount],
		     [AC_DEFINE([HAVE_CUDART], 1, [Define to 1 if we have -lcudart])
		      hwloc_have_cudart=yes])],
       [AC_MSG_RESULT(no)])])

    if test "x$enable_xml" != "xno"; then
        AC_CHECK_PROGS(XMLLINT, [xmllint])
    fi

    AC_CHECK_PROGS(BUNZIPP, bunzip2, false)

    _HWLOC_CHECK_DIFF_U

    # Only generate these files if we're making the tests
    AC_CONFIG_FILES(
        hwloc_config_prefix[tests/Makefile]
        hwloc_config_prefix[tests/linux/Makefile]
        hwloc_config_prefix[tests/linux/gather/Makefile]
        hwloc_config_prefix[tests/xml/Makefile]
        hwloc_config_prefix[tests/ports/Makefile]
        hwloc_config_prefix[tests/linux/hwloc-gather-topology]
        hwloc_config_prefix[tests/linux/gather/test-gather-topology.sh]
        hwloc_config_prefix[tests/linux/test-topology.sh]
        hwloc_config_prefix[tests/xml/test-topology.sh]
        hwloc_config_prefix[utils/test-hwloc-calc.sh]
        hwloc_config_prefix[utils/test-hwloc-distrib.sh])

    AC_CONFIG_COMMANDS([chmoding-scripts], [chmod +x ]hwloc_config_prefix[tests/linux/test-topology.sh ]hwloc_config_prefix[tests/xml/test-topology.sh ]hwloc_config_prefix[tests/linux/hwloc-gather-topology ]hwloc_config_prefix[tests/linux/gather/test-gather-topology.sh ]hwloc_config_prefix[utils/test-hwloc-calc.sh ]hwloc_config_prefix[utils/test-hwloc-distrib.sh])

    # These links are only needed in standalone mode.  It would
    # be nice to m4 foreach this somehow, but whenever I tried
    # it, I got obscure "invalid tag" errors from
    # AC_CONFIG_LINKS.  :-\ Since these tests are only run when
    # built in standalone mode, only generate them in
    # standalone mode.
    AC_CONFIG_LINKS(
        hwloc_config_prefix[tests/ports/topology.c]:hwloc_config_prefix[src/topology.c]
	hwloc_config_prefix[tests/ports/traversal.c]:hwloc_config_prefix[src/traversal.c]
	hwloc_config_prefix[tests/ports/topology-synthetic.c]:hwloc_config_prefix[src/topology-synthetic.c]
	hwloc_config_prefix[tests/ports/topology-solaris.c]:hwloc_config_prefix[src/topology-solaris.c]
	hwloc_config_prefix[tests/ports/topology-aix.c]:hwloc_config_prefix[src/topology-aix.c]
	hwloc_config_prefix[tests/ports/topology-osf.c]:hwloc_config_prefix[src/topology-osf.c]
	hwloc_config_prefix[tests/ports/topology-windows.c]:hwloc_config_prefix[src/topology-windows.c]
	hwloc_config_prefix[tests/ports/topology-darwin.c]:hwloc_config_prefix[src/topology-darwin.c]
	hwloc_config_prefix[tests/ports/topology-freebsd.c]:hwloc_config_prefix[src/topology-freebsd.c]
	hwloc_config_prefix[tests/ports/topology-hpux.c]:hwloc_config_prefix[src/topology-hpux.c])
    ])
])dnl
