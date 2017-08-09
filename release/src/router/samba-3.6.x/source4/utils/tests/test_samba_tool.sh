#!/bin/sh
# Blackbox tests for samba-tool

SERVER=$1
USERNAME=$2
PASSWORD=$3
DOMAIN=$4
shift 4

failed=0

samba4bindir="$BUILDDIR/bin"
smbclient="$samba4bindir/smbclient$EXEEXT"
samba_tool="$samba4bindir/samba-tool$EXEEXT"

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

testit "Test login with --machine-pass without kerberos" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp --machine-pass -k no

testit "Test login with --machine-pass and kerberos" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp --machine-pass -k yes

testit "time" $VALGRIND $samba_tool time $SERVER $CONFIGURATION  -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@

# FIXME: testit "domainlevel.show" $VALGRIND $samba_tool domainlevel show $CONFIGURATION

exit $failed
