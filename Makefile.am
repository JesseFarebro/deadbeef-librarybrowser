#SUBDIRS =   intl\
#            po

#INTLTOOL_FILES = \
#		intltool-extract.in \
#		intltool-merge.in \
#		intltool-update.in

#DISTCLEANFILES = \
#		intltool-extract \
#		intltool-merge \
#       intltool-update \
#       po/.intltool-merge-cache

filebrowser_SOURCES =   filebrowser.c filebrowser.h \
                        support.c support.h \
                        utils.c utils.h

if STATICLINK
pkglib_LTLIBRARIES = ddb_misc_filebrowser.la ddb_misc_filebrowser.fallback.la
else
pkglib_LTLIBRARIES = ddb_misc_filebrowser.la
endif

# normal lib
ddb_misc_filebrowser_la_SOURCES = $(filebrowser_SOURCES)
ddb_misc_filebrowser_la_LDFLAGS = -module
ddb_misc_filebrowser_la_LIBADD  = $(LDADD) $(GTK_DEPS_LIBS)
ddb_misc_filebrowser_la_CFLAGS  = -std=c99 $(GTK_DEPS_CFLAGS) -Wall

# fallback lib
if STATICLINK
GTK_ROOT = /usr

ddb_misc_filebrowser_fallback_la_SOURCES = $(filebrowser_SOURCES)
ddb_misc_filebrowser_fallback_la_LDFLAGS = -module
ddb_misc_filebrowser_fallback_la_LIBADD = $(LDADD) -L$(GTK_ROOT)/lib $(GTK_ROOT)/lib/libgtk-x11-2.0.la $(GTK_ROOT)/lib/libgdk-x11-2.0.la $(GTK_ROOT)/lib/libpangoft2-1.0.la $(GTK_ROOT)/lib/libpangocairo-1.0.la -lm $(GTK_ROOT)/lib/libcairo.la $(GTK_ROOT)/lib/libpango-1.0.la $(GTK_ROOT)/lib/libgobject-2.0.la $(GTK_ROOT)/lib/libgmodule-2.0.la $(GTK_ROOT)/lib/libgthread-2.0.la -lrt $(GTK_ROOT)/lib/libglib-2.0.la
ddb_misc_filebrowser_fallback_la_CFLAGS = -std=c99 -I $(GTK_ROOT)/include -I $(GTK_ROOT)/lib/gtk-2.0/include -I $(GTK_ROOT)/include/glib-2.0 -I $(GTK_ROOT)/include/gtk-2.0 -I $(GTK_ROOT)/include/cairo -I $(GTK_ROOT)/lib/glib-2.0/include/ -I $(GTK_ROOT)/include/pango-1.0 -I $(GTK_ROOT)/include/atk-1.0 -DULTRA_COMPATIBLE=1
endif

docsdir = $(docdir)
docs_DATA = README COPYING

EXTRA_DIST =    $(docs_DATA) \
                #$(INTLTOOL_FILES) \
                userinstall.sh quickinstall.sh

ACLOCAL_AMFLAGS = -I m4
