#!/bin/sh

if test x$srcdir = x; then
	srcdir=.
fi

tmpdir=$top_builddir/tests/tmp/

# remove test-framework
rm -rf $tmpdir

printf "%-40s" "cleaning up"

exit 0
