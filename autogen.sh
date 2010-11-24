#!/bin/sh

aclocal -I m4
autoconf
autoheader
libtoolize --copy --force
automake --add-missing --copy


