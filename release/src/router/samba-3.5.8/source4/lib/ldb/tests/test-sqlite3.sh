#!/bin/sh


LDB_URL="sqlite3://sqltest.ldb"
export LDB_URL

rm -f sqltest.ldb

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

PATH=bin:$PATH
export PATH

LDB_SPECIALS=0
export LDB_SPECIALS

$LDBDIR/tests/test-generic.sh

#. $LDBDIR/tests/test-extended.sh

#. $LDBDIR/tests/test-tdb-features.sh

