#! /bin/sh
set -e

#
# usage: util/checkasm.sh [directory]
#
# This script runs lzotest with all assembler decompressors
# on a complete directory tree.
# It is not suitable for accurate timings.
#
# Copyright (C) 1996-2014 Markus Franz Xaver Johannes Oberhumer
#

LZOTEST="lzotest"
test -x ./lzotest/lzotest && LZOTEST="./lzotest/lzotest"
test -x ./lzotest.exe && LZOTEST="./lzotest.exe"
test -x ./lzotest.out && LZOTEST="./lzotest.out"
LFLAGS="-q"

dir="${*-.}"

TMPFILE="/tmp/lzo_$$.tmp"
rm -f $TMPFILE
(find $dir/ -type f -print > $TMPFILE) || true

for i in 11; do
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -A
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -A -S
done

for i in 61; do
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -F
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -F -S
done

for i in 71 81; do
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -A
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -A -S
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -F
    cat $TMPFILE | $LZOTEST -m${i} -@ $LFLAGS -F -S
done

rm -f $TMPFILE
echo "Done."
exit 0

