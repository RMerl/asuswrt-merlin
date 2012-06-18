#!/bin/sh

LDB_URL="tdb://schema.ldb"
export LDB_URL

rm -f schema.ldb

echo "LDB_URL: $LDB_URL"

echo "Adding schema"
$VALGRIND bin/ldbadd $LDBDIR/tests/schema-tests/schema.ldif || exit 1

echo "Adding few test elements (no failure expected here)"
$VALGRIND bin/ldbadd $LDBDIR/tests/schema-tests/schema-add-test.ldif || exit 1

echo "Modifying elements (2 failures expected here)"

$VALGRIND bin/ldbmodify $LDBDIR/tests/schema-tests/schema-mod-test-1.ldif || exit 1
$VALGRIND bin/ldbmodify $LDBDIR/tests/schema-tests/schema-mod-test-2.ldif || exit 1
$VALGRIND bin/ldbmodify $LDBDIR/tests/schema-tests/schema-mod-test-3.ldif || exit 1
$VALGRIND bin/ldbmodify $LDBDIR/tests/schema-tests/schema-mod-test-4.ldif
if [ "$?" == "0" ]; then
	echo "test failed!"
	exit 1
fi
$VALGRIND bin/ldbmodify $LDBDIR/tests/schema-tests/schema-mod-test-5.ldif
if [ "$?" == "0" ]; then
	echo "test failed!"
	exit 1
fi

echo "Showing modified record"
$VALGRIND bin/ldbsearch '(cn=Test)'  || exit 1

