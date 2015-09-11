#!/bin/sh

if glibtoolize --version &> /dev/null
then libtoolize=glibtoolize
else libtoolize=libtoolize
fi

aclocal -I m4
autoconf
autoheader
$libtoolize --copy --force
automake --add-missing --copy

