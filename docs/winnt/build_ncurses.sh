#!/usr/bin/env bash

# Scripts are provided mainly to document the process and may
# not be the best way of automated it.

# Run from extracted ncurses source directory
# Define HOST_PLATFORM and MINGW_ROOT
source ../env.sh

./configure \
	--host=$HOST_PLATFORM \
	--prefix=$MINGW_ROOT \
	--disable-getcap \
	--disable-hard-tabs \
	--disable-home-terminfo \
	--disable-leaks \
	--disable-macros \
	--disable-overwrite \
	--disable-pc-files \
	--disable-shared \
	--disable-termcap \
	--enable-assertions \
	--enable-database \
	--enable-echo   \
	--enable-exp-win32 \
	--enable-ext-funcs \
	--enable-interop \
	--enable-opaque-curses \
	--enable-opaque-form \
	--enable-opaque-menu \
	--enable-opaque-panel \
	--enable-sp-funcs \
	--enable-static\
	--enable-term-driver \
	--enable-warnings \
	--enable-widec \
	--with-fallbacks=ms-terminal \
	--with-normal \
	--with-progs \
	--without-ada \
	--without-debug \
	--without-libtool \
	--without-manpages \
	--without-shared \

# standard 'make' and 'make install' from here
