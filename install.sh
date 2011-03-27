#!/bin/bash

TARGET="${HOME}/.local/lib/deadbeef"

/bin/mkdir -p "${TARGET}"
/usr/bin/install -v -c -m 644 ./filebrowser.so "${TARGET}/filebrowser.so"
