#!/bin/sh

aclocal -I m4
autoconf
autoheader
libtoolize --copy
automake --add-missing --copy


