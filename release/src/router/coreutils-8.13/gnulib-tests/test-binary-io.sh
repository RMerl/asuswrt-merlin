#!/bin/sh

tmpfiles=""
trap 'rm -fr $tmpfiles' 1 2 3 15

tmpfiles="$tmpfiles t-bin-out1.tmp t-bin-out2.tmp"
./test-binary-io${EXEEXT} > t-bin-out1.tmp || exit 1

rm -fr $tmpfiles

exit 0
