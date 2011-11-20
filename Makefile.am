
all-local:
	git log > ChangeLog
	git shortlog -n -e > AUTHORS

release: all
	./makerelease.sh

#SUBDIRS = \
#	intl\
#	po

#INTLTOOL_FILES = \
#	intltool-extract.in \
#	intltool-merge.in \
#	intltool-update.in

#DISTCLEANFILES = \
#	intltool-extract \
#	intltool-merge \
#	intltool-update \
#	po/.intltool-merge-cache

filebrowser_SOURCES = \
	filebrowser.c filebrowser.h \
	support.c support.h \
	utils.c utils.h

if STATICLINK
pkglib_LTLIBRARIES = ddb_misc_filebrowser_GTK2.la ddb_misc_filebrowser_GTK2.fallback.la
else
if HAVE_GTK2
if HAVE_GTK3
pkglib_LTLIBRARIES = ddb_misc_filebrowser_GTK2.la ddb_misc_filebrowser_GTK3.la
else
pkglib_LTLIBRARIES = ddb_misc_filebrowser_GTK2.la
endif
else
if HAVE_GTK3
pkglib_LTLIBRARIES = ddb_misc_filebrowser_GTK3.la
endif
endif
endif

# normal lib
if HAVE_GTK2
ddb_misc_filebrowser_GTK2_la_SOURCES = $(filebrowser_SOURCES)
ddb_misc_filebrowser_GTK2_la_LDFLAGS = -module
ddb_misc_filebrowser_GTK2_la_LIBADD  = $(LDADD) $(GTK2_DEPS_LIBS)
ddb_misc_filebrowser_GTK2_la_CFLAGS  = -std=c99 $(GTK2_DEPS_CFLAGS) -Wall
endif
if HAVE_GTK3
ddb_misc_filebrowser_GTK3_la_SOURCES = $(filebrowser_SOURCES)
ddb_misc_filebrowser_GTK3_la_LDFLAGS = -module
ddb_misc_filebrowser_GTK3_la_LIBADD  = $(LDADD) $(GTK3_DEPS_LIBS)
ddb_misc_filebrowser_GTK3_la_CFLAGS  = -std=c99 $(GTK3_DEPS_CFLAGS) -Wall
endif

# fallback lib - this is not supposed to work ;-)
if STATICLINK
GTK_ROOT = /usr

ddb_misc_filebrowser_GTK2_fallback_la_SOURCES = $(filebrowser_SOURCES)
ddb_misc_filebrowser_GTK2_fallback_la_LDFLAGS = -module
ddb_misc_filebrowser_GTK2_fallback_la_LIBADD = $(LDADD) -L$(GTK_ROOT)/lib $(GTK_ROOT)/lib/libgtk-x11-2.0.la $(GTK_ROOT)/lib/libgdk-x11-2.0.la $(GTK_ROOT)/lib/libpangoft2-1.0.la $(GTK_ROOT)/lib/libpangocairo-1.0.la -lm $(GTK_ROOT)/lib/libcairo.la $(GTK_ROOT)/lib/libpango-1.0.la $(GTK_ROOT)/lib/libgobject-2.0.la $(GTK_ROOT)/lib/libgmodule-2.0.la $(GTK_ROOT)/lib/libgthread-2.0.la -lrt $(GTK_ROOT)/lib/libglib-2.0.la
ddb_misc_filebrowser_GTK2_fallback_la_CFLAGS = -std=c99 -I $(GTK_ROOT)/include -I $(GTK_ROOT)/lib/gtk-2.0/include -I $(GTK_ROOT)/include/glib-2.0 -I $(GTK_ROOT)/include/gtk-2.0 -I $(GTK_ROOT)/include/cairo -I $(GTK_ROOT)/lib/glib-2.0/include/ -I $(GTK_ROOT)/include/pango-1.0 -I $(GTK_ROOT)/include/atk-1.0 -DULTRA_COMPATIBLE=1
endif

docsdir = $(docdir)
docs_DATA = README COPYING AUTHORS ChangeLog

dist_data_DATA = 

EXTRA_DIST = \
	userinstall.sh quickinstall.sh userremove.sh quickremove.sh \
	$(docs_DATA)
#$(INTLTOOL_FILES)

ACLOCAL_AMFLAGS = -I m4

