#! /bin/csh -f
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright © 2010 INRIA.  All rights reserved.
# Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

set builddir="`pwd`"

set srcdir="$1"
cd "$srcdir"
set srcdir=`pwd`
cd "$builddir"

set distdir="$builddir/$2"
set HWLOC_VERSION="$3"
set HWLOC_REPO_REV="$4"

if ("$distdir" == "") then
    echo "Must supply relative distdir as argv[2] -- aborting"
    exit 1
elif ("$HWLOC_VERSION" == "") then
    echo "Must supply version as argv[1] -- aborting"
    exit 1
endif

#========================================================================

if ("$srcdir" != "$builddir") then
    set vpath=1
    set vpath_msg=yes
else
    set vpath=0
    set vpath_msg=no
endif

# We can catch some hard (but possible) to do mistakes by looking at
# our tree's revision number, but only if we are in the source tree.
# Otherwise, use what configure told us, at the cost of allowing one
# or two corner cases in (but otherwise VPATH builds won't work).
set repo_rev=$HWLOC_REPO_REV
if (-d .svn) then
    set repo_rev="r`svnversion .`"
endif

set start=`date`
cat <<EOF
 
Creating hwloc distribution
In directory: `pwd`
Srcdir: $srcdir
Builddir: $builddir
VPATH: $vpath_msg
Version: $HWLOC_VERSION
Started: $start
 
EOF

umask 022

if (! -d "$distdir") then
    echo "*** ERROR: dist dir does not exist"
    echo "*** ERROR:   $distdir"
    exit 1
endif

#
# See if we need to update the version file with the current repo
# revision number.  Do this *before* entering the distribution tree to
# solve a whole host of problems with VPATH (since srcdir may be
# relative or absolute)
#
set cur_repo_rev="`grep '^repo_rev' ${distdir}/VERSION | cut -d= -f2`"
if ("$cur_repo_rev" == "-1") then
    sed -e 's/^repo_rev=.*/repo_rev='$repo_rev'/' "${distdir}/VERSION" > "${distdir}/version.new"
    cp "${distdir}/version.new" "${distdir}/VERSION"
    rm -f "${distdir}/version.new"
    # need to reset the timestamp to not annoy AM dependencies
    touch -r "${srcdir}/VERSION" "${distdir}/VERSION"
    echo "*** Updated VERSION file with repo rev number: $repo_rev"
else
    echo "*** Did NOT update VERSION file with repo rev number"
endif

#
# VPATH builds only work if the srcdir has valid docs already built.
# If we're VPATH and the srcdir doesn't have valid docs, then fail.
#

if ($vpath == 1 && ! -d $srcdir/doc/doxygen-doc) then
    echo "*** This is a VPATH 'make dist', but the srcdir does not already"
    echo "*** have a doxygen-doc tree built.  hwloc's config/distscript.csh"
    echo "*** requores the docs to be built in the srcdir before executing"
    echo "*** 'make dist' in a VPATH build."
    exit 1
endif

#
# If we're not VPATH, force the generation of new doxygen documentation
#

if ($vpath == 0) then
    # Not VPATH
    echo "*** Making new doxygen documentation (doxygen-doc tree)"
    echo "*** Directory: srcdir: $srcdir, distdir: $distdir, pwd: `pwd`"
    cd doc
    # We're still in the src tree, so kill any previous doxygen-docs
    # tree and make a new one.
    chmod -R a=rwx doxygen-doc
    rm -rf doxygen-doc
    make
    if ($status != 0) then
        echo ERROR: generating doxygen docs failed
        echo ERROR: cannot continue
        exit 1
    endif

    # Make new README file
    echo "*** Making new README"
    make readme
    if ($status != 0) then
        echo ERROR: generating new README failed
        echo ERROR: cannot continue
        exit 1
    endif
else
    echo "*** This is a VPATH build; assuming that the doxygen docs and REAME"
    echo "*** are current in the srcdir (i.e., we'll just copy those)"
endif

echo "*** Copying doxygen-doc tree to dist..."
echo "*** Directory: srcdir: $srcdir, distdir: $distdir, pwd: `pwd`"
chmod -R a=rwx $distdir/doc/doxygen-doc
echo rm -rf $distdir/doc/doxygen-doc
rm -rf $distdir/doc/doxygen-doc
echo cp -rpf $srcdir/doc/doxygen-doc $distdir/doc
cp -rpf $srcdir/doc/doxygen-doc $distdir/doc

echo "*** Copying new README"
ls -lf $distdir/README
cp -pf $srcdir/README $distdir

#########################################################
# VERY IMPORTANT: Now go into the new distribution tree #
#########################################################
cd "$distdir"
echo "*** Now in distdir: $distdir"

#
# Remove all the latex source files from the distribution tree (the
# PDFs are still there; we're just removing the latex source because
# some of the filenames get really, really long...).
#

echo "*** Removing latex source from dist tree"
rm -rf doc/doxygen-doc/latex

#
# Get the latest config.guess and config.sub from ftp.gnu.org
#

echo "*** Downloading latest config.sub/config.guess from ftp.gnu.org..."
cd config
set configdir="`pwd`"
mkdir tmp.$$
cd tmp.$$
# Official HTTP git mirrors for config.guess / config.sub
wget -t 1 -T 10 -O config.guess 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=master'
wget -t 1 -T 10 -O config.sub 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=master'
chmod +x config.guess config.sub

# Recently, ftp.gnu.org has had zero-legnth config.guess / config.sub
# files, which causes the automated nightly SVN snapshot tarball to
# fail to be made correctly.  This is a primitive attempt to fix that.
# If we got zero-length files from wget, use a config.guess /
# config.sub from a known location that is more recent than what ships
# in the current generation of auto* tools.  Also check to ensure that
# the resulting scripts are runnable (Jan 2009: there are un-runnable
# scripts available right now because of some git vulnerability).

# Before you complain about this too loudly, remember that we're using
# unreleased software...

set happy=0
if (! -f config.guess || ! -s config.guess) then
    echo " - WARNING: Got bad config.guess from ftp.gnu.org (non-existent or empty)"
else
    ./config.guess >& /dev/null
    if ($status != 0) then
        echo " - WARNING: Got bad config.guess from ftp.gnu.org (not executable)"
    else
        if (! -f config.sub || ! -s config.sub) then
            echo " - WARNING: Got bad config.sub from ftp.gnu.org (non-existent or empty)"
        else
            ./config.sub `./config.guess` >& /dev/null
            if ($status != 0) then
                echo " - WARNING: Got bad config.sub from ftp.gnu.org (not executable)"
            else
                echo " - Got good config.guess and config.sub from ftp.gnu.org"
                chmod +w ../config.sub ../config.guess
                cp config.sub config.guess ..
                set happy=1
            endif
        endif
    endif
endif

if ("$happy" == "0") then
    echo " - WARNING: using included versions for both config.sub and config.guess"
endif
cd ..
rm -rf tmp.$$
cd ..

#
# All done
#

cat <<EOF
*** hwloc version $HWLOC_VERSION distribution created
 
Started: $start
Ended:   `date`
 
EOF

