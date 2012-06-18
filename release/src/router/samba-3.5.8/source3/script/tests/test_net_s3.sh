#!/bin/sh

# tests for the "net" command

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

failed=0

net_misc() {
	echo "RUNNING SUBTESTS net_misc"
	$SCRIPTDIR/test_net_misc.sh \
	|| failed=`expr $failed + $?`
}

net_registry() {
	echo "RUNNING SUBTESTS net_registry"
	$SCRIPTDIR/test_net_registry.sh \
	|| failed=`expr $failed + $?`
}

net_rpc_registry() {
	echo "RUNNING SUBTESTS net_rpc_registry"
	$SCRIPTDIR/test_net_registry.sh rpc \
	|| failed=`expr $failed + $?`
}

net_misc
net_registry
net_rpc_registry

testok $0 $failed

