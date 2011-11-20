#!/bin/sh

## Remove old versions
rm -fv /usr/local/lib/deadbeef/ddb_misc_filebrowser.so*
rm -fv /usr/local/lib/deadbeef/ddb_misc_filebrowser_GTK2.so*
rm -fv /usr/local/lib/deadbeef/ddb_misc_filebrowser_GTK3.so*

## GTK2 version
if [ -f ./.libs/ddb_misc_filebrowser_GTK2.so ]; then
    /usr/bin/install -v -c -m 644 ./.libs/ddb_misc_filebrowser_GTK2.so /usr/local/lib/deadbeef/
else
    /usr/bin/install -v -c -m 644 ./ddb_misc_filebrowser_GTK2.so /usr/local/lib/deadbeef/
fi

## GTK3 version
if [ -f ./.libs/ddb_misc_filebrowser_GTK3.so ]; then
    /usr/bin/install -v -c -m 644 ./.libs/ddb_misc_filebrowser_GTK3.so /usr/local/lib/deadbeef/
else
    /usr/bin/install -v -c -m 644 ./ddb_misc_filebrowser_GTK3.so /usr/local/lib/deadbeef/
fi

if [ -f ${HOME}/.local/lib/deadbeef/ddb_misc_filebrowser.so ]; then
    echo "Warning: An old version of the filebrowser plugin is present in ${HOME}/.local/lib/deadbeef/, you should remove it to avoid conflicts!"
fi

if [ -f ${HOME}/.local/lib/deadbeef/ddb_misc_filebrowser_GTK2.so -o -f ${HOME}/.local/lib/deadbeef/ddb_misc_filebrowser_GTK3.so ]; then
    echo "Warning: Some version of the filebrowser plugin is present in ${HOME}/.local/lib/deadbeef/, you should remove it to avoid conflicts!"
fi
