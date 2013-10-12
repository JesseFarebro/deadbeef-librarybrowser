#!/bin/bash

FLAG=$1

OLDPWD=`pwd`

PACKAGENAME=deadbeef-fb
DISTPACKAGENAME=deadbeef-devel
INSTALLDIR=${OLDPWD}/../install

DATE=`date +%Y%m%d`
BINTARGET=${OLDPWD}/../${PACKAGENAME}${FLAG}_${DATE}.tar.gz
SRCTARGET=${OLDPWD}/../${PACKAGENAME}${FLAG}_${DATE}_src.tar.gz

rm -rf ${INSTALLDIR}
make DESTDIR=${INSTALLDIR} install
if [ -d ${INSTALLDIR} ]; then
    cd ${INSTALLDIR}
    tar -czf $BINTARGET ./
    cd ${OLDPWD}
fi

rm -f ${DISTPACKAGENAME}.tar.gz
make dist && mv ${DISTPACKAGENAME}.tar.gz ${SRCTARGET}

ls -lh ${SRCTARGET} ${BINTARGET}
