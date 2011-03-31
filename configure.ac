dnl Process this file with autoconf to produce a configure script.
AC_INIT([deadbeef-fb],[devel])
AM_INIT_AUTOMAKE()

dnl Override $PACKAGE to make $pkgXdir use deadbeef search path
PACKAGE=deadbeef

#AM_CONFIG_HEADER(config.h)

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_LANG_C
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])
PKG_PROG_PKG_CONFIG

#IT_PROG_INTLTOOL([0.40.0])
#AM_GNU_GETTEXT
#GETTEXT_PACKAGE=deadbeef-fb
#AC_SUBST(GETTEXT_PACKAGE)

test "x$prefix" = xNONE && prefix=$ac_default_prefix
AC_DEFINE_UNQUOTED(LOCALEDIR, "${prefix}/${DATADIRNAME}/locale", [Name of gettext locale directory])

case "$host" in
  i386-*-* | i486-*-* | i586-*-* | i686-*-* | i86pc-*-*)
    AC_DEFINE(ARCH_X86_32, 1, [architecture is x86])
    LIB="lib-x86-32"
    ;;
  x86_64-apple-*)
    AC_DEFINE(ARCH_X86_64_OSX, 1, [architecture is x86_64 for OSX])
    LIB="lib-x86-64-osx"
    ;;
  x86_64-*-* | amd64-*-*)
    AC_DEFINE(ARCH_X86_64, 1, [architecture is x86_64])
    LIB="lib-x86-64"
    ;;
  powerpc-*-* )
    AC_DEFINE(ARCH_PPC_32, 1, [architecture is ppc32])
    LIB="lib-ppc-32"
    ;;
  powerpc64-*-* )
    AC_DEFINE(ARCH_PPC_64, 1, [architecture is ppc64])
    LIB="lib-ppc-64"
    ;;
  *)
    AC_DEFINE(ARCH_UNKNOWN, 1, [architecture is unknown])
    ;;
esac

AC_ARG_ENABLE(staticlink, [AS_HELP_STRING([--enable-staticlink], [link everything statically (default: disabled)])], [enable_staticlink=$enableval], [enable_staticlink=no])

if test "x$enable_staticlink" != "xno" ; then
    AC_DEFINE_UNQUOTED([STATICLINK], [1], [Define if building static version])
    STATICLINK=yes
fi

PKG_CHECK_MODULES(GTK_DEPS, gtk+-2.0 >= 2.12 gthread-2.0 glib-2.0,,
    AC_MSG_ERROR(gtk+-2.0 headers missing))

CFLAGS+=" $(pkg-config --cflags gtk+-2.0)"
AC_CHECK_HEADERS([deadbeef/deadbeef.h gtk/gtk.h deadbeef/gtkui_api.h],,
    AC_MSG_ERROR(deadbeef headers missing),
    [#ifdef HAVE_DEADBEEF_DEADBEEF_H
    #include <deadbeef/deadbeef.h>
    #endif
    #ifdef HAVE_GTK_GTK_H
    #include <gtk/gtk.h>
    #endif
    ])

AM_CONDITIONAL(STATICLINK, test "x$STATICLINK" = "xyes")

AC_CONFIG_FILES([
Makefile
])
#intl/Makefile
#po/Makefile.in

dnl read Makefile.in and write Makefile
AC_OUTPUT
