#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTDIR=$(dirname "$SCRIPT")
SOURCEDIR="$SCRIPTDIR/.."

PKG_NL3=$(pkg-config --cflags libnl-3.0)

IWYU=${IWYU:-iwyu}

cd "$SOURCEDIR" || exit

make clean
make -k -s CC="$IWYU" CFLAGS="-Xiwyu --no_comments -Xiwyu --no_fwd_decl -Xiwyu --mapping_file='$SCRIPTDIR/htop.imp' $PKG_NL3"
