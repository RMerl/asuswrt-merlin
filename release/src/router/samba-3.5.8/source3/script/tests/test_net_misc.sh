#!/bin/sh

# various tests for the "net" command

NET="$VALGRIND ${NET:-$BINDIR/net} $CONFIGURATION"

NETTIME="${NET} time"
NETLOOKUP="${NET} lookup"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

failed=0

test_time()
{
	PARAM="$1"

	${NETTIME} -S ${SERVER} ${PARAM}
}

test_lookup()
{
	PARAM="$1"

	${NETLOOKUP} ${PARAM}
}

testit "get the time" \
	test_time || \
	failed=`expr $failed + 1`

testit "get the system time" \
	test_time system || \
	failed=`expr $failed + 1`

testit "get the time zone" \
	test_time zone || \
	failed=`expr $failed + 1`

testit "lookup the PDC" \
	test_lookup pdc || \
	failed=`expr $failed + 1`

testit "lookup the master browser" \
	test_lookup master || \
	failed=`expr $failed + 1`

testok $0 $failed

