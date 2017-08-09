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

cat <<EOF | $VALGRIND ldbadd$EXEEXT || exit 1
dn: @MODULES
@LIST: rdn_name
EOF

$VALGRIND ldbadd$EXEEXT $LDBDIR/tests/init.ldif || exit 1

. $LDBDIR/tests/test-generic.sh

. $LDBDIR/tests/test-extended.sh

. $LDBDIR/tests/test-tdb-features.sh

. $LDBDIR/tests/test-controls.sh
