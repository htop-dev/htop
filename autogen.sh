#!/bin/sh

glibtoolize=$(which glibtoolize 2> /dev/null)
if [ ${#glibtoolize} -gt 0 ]
then libtoolize=glibtoolize
else libtoolize=libtoolize
fi

mkdir -p m4
aclocal -I m4
autoconf
autoheader
$libtoolize --copy --force
automake --add-missing --copy

