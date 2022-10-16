#!/usr/bin/env bash




source ~/work/env.sh

./configure \
	--host=$HOST_PLATFORM \
        --prefix=$MINGW_ROOT \
        --without-ada \
        --enable-warnings \
        --enable-assertions \
        --enable-exp-win32 \
        --enable-ext-funcs \
        --disable-home-terminfo \
        --disable-echo \
        --disable-getcap \
        --disable-hard-tabs \
        --disable-leaks \
        --disable-macros \
        --disable-overwrite \
        --enable-opaque-curses \
        --enable-opaque-panel \
        --enable-opaque-menu \
        --enable-opaque-form \
        --enable-database \
        --enable-sp-funcs \
        --enable-term-driver \
        --enable-interop \
        --disable-termcap \
        --enable-database \
        --with-progs \
        --without-libtool \
        --with-shared \
        --with-normal \
        --without-debug \
        --with-fallbacks=ms-terminal \
        --without-manpages \
	--enable-widec

