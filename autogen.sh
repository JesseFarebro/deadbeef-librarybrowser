#!/bin/sh
git log > ChangeLog
aclocal
autoheader
libtoolize
autoconf
automake -a -c
