#!/bin/sh
aclocal
autoheader
libtoolize
#intltoolize
autoconf
automake -a -c
