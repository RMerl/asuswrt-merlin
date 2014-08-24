#!/bin/ksh

# so-thresh.sh
#
# This script is a "wrapper" to use the so-thresh.mk
# Makefile.  See the comments in so-thresh.exp
# regarding why this script exists.

#set -o xtrace
#set -o verbose

if [ "$srcdir" = "${srcdir#/}" ]
then
    srcdir="$PWD/$srcdir"
fi

if [ "$objdir" = "${objdir#/}" ]
then
    objdir="$PWD/$objdir"
fi

subdir="$1"

HERE=$PWD
cd $subdir

MAKEFLAGS=
make -f ${srcdir}/${subdir}/so-thresh.mk clean require_shlibs all SRCDIR=${srcdir}/${subdir} OBJDIR=${objdir}/${subdir} > ${objdir}/${subdir}/so-thresh.make.out 2>&1
STATUS=$?

cd $HERE
echo "return STATUS is $STATUS"

exit $STATUS
