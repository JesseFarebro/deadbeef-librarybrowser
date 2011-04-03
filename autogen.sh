#!/bin/sh
git log > ChangeLog
git shortlog -s > AUTHORS

aclocal
autoheader
libtoolize
autoconf
automake -a -c
