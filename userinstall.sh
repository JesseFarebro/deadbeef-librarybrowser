#!/bin/sh

[ ! -f ./ddb_misc_filebrowser.so ] && ln -sf .libs/ddb_misc_filebrowser.so ./

/bin/mkdir -p ${HOME}/.local/lib/deadbeef
/usr/bin/install -v -c -m 644 ./.libs/ddb_misc_filebrowser.so ${HOME}/.local/lib/deadbeef

[ -f /usr/local/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in /usr/local/lib/deadbeef, you should remove it to avoid conflicts"
[ -f /usr/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in /usr/lib/deadbeef, you should remove it to avoid conflicts"
