#!/bin/sh

git log --format='%aN' | sort -u > AUTHORS
git log > ChangeLog

aclocal
autoheader
libtoolize
#intltoolize
autoconf
automake -a -c
