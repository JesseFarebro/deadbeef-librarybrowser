#!/bin/sh

/bin/mkdir -p ${HOME}/.local/lib/deadbeef

if [ -f ./.libs/ddb_misc_filebrowser.so ]; then
    /usr/bin/install -v -c -m 644 ./.libs/ddb_misc_filebrowser.so ${HOME}/.local/lib/deadbeef/
else
    /usr/bin/install -v -c -m 644 ./ddb_misc_filebrowser.so ${HOME}/.local/lib/deadbeef/
fi

[ -f /usr/local/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in /usr/local/lib/deadbeef, you should remove it to avoid conflicts"
[ -f /usr/lib/deadbeef/ddb_misc_filebrowser.so ] && \
    echo "Warning: File ddb_misc_filebrowser.so is present in /usr/lib/deadbeef, you should remove it to avoid conflicts"
