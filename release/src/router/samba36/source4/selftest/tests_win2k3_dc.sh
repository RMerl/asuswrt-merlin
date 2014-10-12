#!/bin/sh

if [ ! $WINTESTCONF ]; then
	echo "Environment variable WINTESTCONF has not been defined."
	echo "Windows tests will not run unconfigured."
	exit 1
fi

if [ ! -r $WINTESTCONF ]; then
	echo "$WINTESTCONF could not be read."
	exit 1
fi

. selftest/test_functions.sh

export SRCDIR=$SRCDIR

tests="RPC-DRSUAPI RPC-SPOOLSS ncacn_np ncacn_ip_tcp"

for name in $tests; do
	testit $name rpc $SRCDIR/selftest/win/wintest_2k3_dc.sh $name
done
