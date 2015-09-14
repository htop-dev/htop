#!/bin/sh

glibtoolize=$(which glibtoolize)
if [ ${#glibtoolize} -gt 0 ]
then libtoolize=glibtoolize
else libtoolize=libtoolize
fi

aclocal -I m4
autoconf
autoheader
$libtoolize --copy --force
automake --add-missing --copy

