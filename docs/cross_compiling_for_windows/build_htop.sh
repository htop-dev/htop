#!/usr/bin/env bash

source ~/work/env.sh

./autogen.sh

export CFLAGS="-I$MINGW_ROOT/include"
export LDFLAGS="-L$MINGW_ROOT/lib"


./configure \
	--host=$HOST_PLATFORM \
	--prefix=$MINGW_ROOT \
	--enable-static \
	--enable-unicode \
#	--disable-unicode \	

