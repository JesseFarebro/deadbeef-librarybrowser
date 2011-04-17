#!/bin/sh

git log > ChangeLog
git shortlog -s > AUTHORS

aclocal
autoheader
libtoolize
#intltoolize
autoconf
automake -a -c
