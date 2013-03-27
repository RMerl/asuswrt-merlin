#!/bin/sh

rm -f debug.log
rm -f child.log

# for shared builds.
LD_LIBRARY_PATH=../src/.libs:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

for f in $*; do
    if ./$f; then
	:
    else
	echo FAILURE
	exit 1
    fi
done

exit 0
