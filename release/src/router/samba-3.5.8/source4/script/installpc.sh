#!/bin/sh
# install miscellaneous files

SRCDIR="$1"
PKGCONFIGDIR="$2"
shift
shift

cd $SRCDIR || exit 1

for I in $*
do
	cp $I $PKGCONFIGDIR
done

exit 0
