#!/bin/sh

if [ -n "$TEST_DATA_PREFIX" ]; then
	LDB_URL="$TEST_DATA_PREFIX/tdbtest.ldb"
else
	LDB_URL="tdbtest.ldb"
fi
export LDB_URL

PATH=bin:$PATH
export PATH

rm -f $LDB_URL*

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

cat <<EOF | $VALGRIND ldbadd || exit 1
dn: @MODULES
@LIST: sample
EOF

cat <<EOF | $VALGRIND ldbadd || exit 1
dn: dc=bar
dc: bar
someThing: someThingElse
EOF

$VALGRIND ldbsearch "(touchedBy=sample)" | grep "touchedBy: sample" || exit 1

