#!/bin/sh

NUMPROC=$(getconf _NPROCESSORS_ONLN)
ref=$(git log --pretty="format:%h" -1)
echo ${ref}
sed -i.nogit -E "s/<[a-z0-9]+>/<${ref}>/g" include/version.h
cat include/version.h

TARGET="${1:-release}"
gmake clean && gmake -j ${NUMPROC} -f FreeBSD-amd64.mk ${TARGET}
