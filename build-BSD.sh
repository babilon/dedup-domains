#!/bin/sh

NUMPROC=$(getconf _NPROCESSORS_ONLN)
TARGET="${1:-release}"
gmake -j ${NUMPROC} -f FreeBSD-amd64.mk ${TARGET}
