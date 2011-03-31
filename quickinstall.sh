#!/bin/sh

rm /usr/local/lib/deadbeef/ddb_misc_filebrowser.so*
if [ -f ./.libs/ddb_misc_filebrowser.so ]; then
    /usr/bin/install -v -c -m 644 ./.libs/ddb_misc_filebrowser.so /usr/local/lib/deadbeef/
else
    /usr/bin/install -v -c -m 644 ./ddb_misc_filebrowser.so /usr/local/lib/deadbeef/
fi

[ -f ${HOME}/.local/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in ${HOME}/.local/lib/deadbeef/, you should remove it to avoid conflicts"
