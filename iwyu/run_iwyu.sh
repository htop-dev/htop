#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")
SOURCEDIR="$SCRIPTDIR/.."

PKG_NL3=$(pkg-config --cflags libnl-3.0)

cd "$SOURCEDIR"

make clean
make -k CC="iwyu" CFLAGS="-Xiwyu --no_comments -Xiwyu --no_fwd_decl -Xiwyu --mapping_file='$SCRIPTDIR/htop.imp' $PKG_NL3"
