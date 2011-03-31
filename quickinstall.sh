#!/bin/sh
[ ! -f ./ddb_misc_filebrowser.so ] && ln -sf .libs/ddb_misc_filebrowser.so ./

rm /usr/local/lib/deadbeef/ddb_misc_filebrowser.so*
cp -v ./.libs/ddb_misc_filebrowser.so /usr/local/lib/deadbeef/

[ -f ${HOME}/.local/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in ${HOME}/.local/lib/deadbeef/, you should remove it to avoid conflicts"
