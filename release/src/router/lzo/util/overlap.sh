#! /bin/sh
set -e

#
# usage: util/overlap.sh [directory]
#
# This script runs the overlap example program on a complete directory tree.
#
# Copyright (C) 1996-2011 Markus Franz Xaver Johannes Oberhumer
#

OVERLAP="overlap"
test -x ./examples/overlap && OVERLAP="./examples/overlap"
test -x ./overlap.exe && OVERLAP="./overlap.exe"

dir="${*-.}"

TMPFILE="/tmp/lzo_$$.tmp"
rm -f $TMPFILE
(find $dir/ -type f -print0 > $TMPFILE) || true

cat $TMPFILE | xargs -0 -r $OVERLAP

rm -f $TMPFILE
echo "Done."
exit 0

