#!/usr/bin/env bash

# Scripts are provided mainly to document the process and may
# not be the best way of automated it.

# Run from cloned htop sources directory.
# (Current work is being down in the 'windows' branch)

# Define HOST_PLATFORM and MINGW_ROOT
source ../env.sh

./autogen.sh

# Make sure configure can find the crossbuilt ncurses files
export CFLAGS="-I$MINGW_ROOT/include"
export LDFLAGS="-L$MINGW_ROOT/lib"

./configure \
	--host=$HOST_PLATFORM \
	--prefix=$MINGW_ROOT \
	--enable-static \
	--enable-unicode
