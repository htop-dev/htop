#!/bin/sh
#
# hwloc_get_version is created from hwloc_get_version.m4 and hwloc_get_version.m4sh.
#
# Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright © 2008-2010 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# 11 September 2009: this file was copied from PLPA's SVN trunk as of
# r251 on 11 September 2009.  The only change made to it was
# s/PLPA/hwloc/ig.


# HWLOC_GET_VERSION(version_file, variable_prefix)
# -----------------------------------------------
# parse version_file for version information, setting
# the following shell variables:
#
#  prefix_VERSION
#  prefix_BASE_VERSION
#  prefix_MAJOR_VERSION
#  prefix_MINOR_VERSION
#  prefix_RELEASE_VERSION
#  prefix_GREEK_VERSION
#  prefix_WANT_REPO_REV
#  prefix_REPO_REV
#  prefix_RELEASE_DATE



srcfile="$1"
option="$2"

case "$option" in
    # svnversion can take a while to run.  If we don't need it, don't run it.
    --major|--minor|--release|--greek|--base|--help)
        ompi_ver_need_repo_rev=0
        ;;
    *)
        ompi_ver_need_repo_rev=1
esac


if test -z "$srcfile"; then
    option="--help"
else

    : ${ompi_ver_need_repo_rev=1}
    : ${srcdir=.}
    : ${svnversion_result=-1}

        if test -f "$srcfile"; then
        ompi_vers=`sed -n "
	t clear
	: clear
	s/^major/HWLOC_MAJOR_VERSION/
	s/^minor/HWLOC_MINOR_VERSION/
	s/^release/HWLOC_RELEASE_VERSION/
	s/^greek/HWLOC_GREEK_VERSION/
	s/^want_repo_rev/HWLOC_WANT_REPO_REV/
	s/^repo_rev/HWLOC_REPO_REV/
	s/^date/HWLOC_RELEASE_DATE/
	t print
	b
	: print
	p" < "$srcfile"`
	eval "$ompi_vers"

        # Only print release version if it isn't 0
        if test $HWLOC_RELEASE_VERSION -ne 0 ; then
            HWLOC_VERSION="$HWLOC_MAJOR_VERSION.$HWLOC_MINOR_VERSION.$HWLOC_RELEASE_VERSION"
        else
            HWLOC_VERSION="$HWLOC_MAJOR_VERSION.$HWLOC_MINOR_VERSION"
        fi
        HWLOC_VERSION="${HWLOC_VERSION}${HWLOC_GREEK_VERSION}"
        HWLOC_BASE_VERSION=$HWLOC_VERSION

        if test $HWLOC_WANT_REPO_REV -eq 1 && test $ompi_ver_need_repo_rev -eq 1 ; then
            if test "$svnversion_result" != "-1" ; then
                HWLOC_REPO_REV=$svnversion_result
            fi
            if test "$HWLOC_REPO_REV" = "-1" ; then

                if test -d "$srcdir/.svn" ; then
                    HWLOC_REPO_REV=r`svnversion "$srcdir"`
                elif test -d "$srcdir/.hg" ; then
                    HWLOC_REPO_REV=hg`hg -v -R "$srcdir" tip | grep changeset | cut -d: -f3`
                elif test -d "$srcdir/.git" ; then
                    HWLOC_REPO_REV=git`git log -1 "$srcdir" | grep commit | awk '{ print $2 }'`
                fi
                if test "HWLOC_REPO_REV" = ""; then
                    HWLOC_REPO_REV=date`date '+%m%d%Y'`
                fi

            fi
            HWLOC_VERSION="${HWLOC_VERSION}${HWLOC_REPO_REV}"
        fi
    fi


    if test "$option" = ""; then
	option="--full"
    fi
fi

case "$option" in
    --full|-v|--version)
	echo $HWLOC_VERSION
	;;
    --major)
	echo $HWLOC_MAJOR_VERSION
	;;
    --minor)
	echo $HWLOC_MINOR_VERSION
	;;
    --release)
	echo $HWLOC_RELEASE_VERSION
	;;
    --greek)
	echo $HWLOC_GREEK_VERSION
	;;
    --repo-rev)
	echo $HWLOC_REPO_REV
	;;
    --base)
        echo $HWLOC_BASE_VERSION
        ;;
    --release-date)
        echo $HWLOC_RELEASE_DATE
        ;;
    --all)
        echo ${HWLOC_VERSION} ${HWLOC_MAJOR_VERSION} ${HWLOC_MINOR_VERSION} ${HWLOC_RELEASE_VERSION} ${HWLOC_GREEK_VERSION} ${HWLOC_REPO_REV}
        ;;
    -h|--help)
	cat <<EOF
$0 <srcfile> <option>

<srcfile> - Text version file
<option>  - One of:
    --full         - Full version number
    --major        - Major version number
    --minor        - Minor version number
    --release      - Release version number
    --greek        - Greek (alpha, beta, etc) version number
    --repo-rev     - Repository version number
    --all          - Show all version numbers, separated by :
    --base         - Show base version number (no repo version number)
    --release-date - Show the release date
    --help         - This message
EOF
        ;;
    *)
        echo "Unrecognized option $option.  Run $0 --help for options"
        ;;
esac

# All done

exit 0
