#!/bin/sh
# Blackbox tests for net

SERVER=$1
USERNAME=$2
PASSWORD=$3
DOMAIN=$4
shift 4

failed=0

samba4bindir="$BUILDDIR/bin"
smbclient="$samba4bindir/smbclient$EXEEXT"
net="$samba4bindir/net$EXEEXT"

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

testit "domain join" $VALGRIND $net join $DOMAIN $CONFIGURATION  -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@ || failed=`expr $failed + 1`

testit "Test login with --machine-pass without kerberos" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp --machine-pass -k no || failed=`expr $failed + 1`

testit "Test login with --machine-pass and kerberos" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp --machine-pass -k yes || failed=`expr $failed + 1`

exit $failed


