#!/bin/sh
# Blackbox tests for nmblookup

NETBIOSNAME=$1
NETBIOSALIAS=$2
SERVER=$3
SERVER_IP=$4
shift 4
TORTURE_OPTIONS=$*

failed=0

testit() {
	name="$1"
	shift
	cmdline="$*"
	echo "test: $name"
	$cmdline
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
		failed=`expr $failed + 1`
	fi
	return $status
}

samba4bindir="$BUILDDIR/bin"
nmblookup="$samba4bindir/nmblookup$EXEEXT"

testit "nmblookup -U \$SERVER_IP \$SERVER" $nmblookup $TORTURE_OPTIONS -U $SERVER_IP $SERVER
testit "nmblookup -U \$SERVER_IP \$NETBIOSNAME" $nmblookup $TORTURE_OPTIONS -U $SERVER_IP $NETBIOSNAME
testit "nmblookup -U \$SERVER_IP \$NETBIOSALIAS" $nmblookup $TORTURE_OPTIONS -U $SERVER_IP $NETBIOSALIAS
testit "nmblookup \$SERVER" $nmblookup $TORTURE_OPTIONS $SERVER
testit "nmblookup \$NETBIOSNAME" $nmblookup $TORTURE_OPTIONS $NETBIOSNAME
testit "nmblookup \$NETBIOSALIAS" $nmblookup $TORTURE_OPTIONS $NETBIOSALIAS

exit $failed
