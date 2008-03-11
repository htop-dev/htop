#!/bin/sh

aclocal
autoconf
autoheader
libtoolize --copy
automake --add-missing --copy


