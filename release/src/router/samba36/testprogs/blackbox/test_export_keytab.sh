#!/bin/sh
# Blackbox tests for kinit and kerberos integration with smbclient etc
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2008 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_extract_keytab.sh SERVER USERNAME REALM DOMAIN PREFIX
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
REALM=$3
DOMAIN=$4
PREFIX=$5
shift 5
failed=0

samba4bindir="$BUILDDIR/bin"
smbclient="$samba4bindir/smbclient$EXEEXT"
samba4kinit="$samba4bindir/samba4kinit$EXEEXT"
samba_tool="$samba4bindir/samba-tool$EXEEXT"
newuser="$samba_tool newuser"

. `dirname $0`/subunit.sh

test_smbclient() {
	name="$1"
	cmd="$2"
	shift
	shift
	echo "test: $name"
	$VALGRIND $smbclient //$SERVER/tmp -c "$cmd" -W "$DOMAIN" $@
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
	fi
	return $status
}

USERPASS=testPaSS@01%

testit "create user locally" $VALGRIND $newuser nettestuser $USERPASS $@ || failed=`expr $failed + 1`

testit "export keytab from domain" $VALGRIND $samba_tool export keytab $PREFIX/tmpkeytab $@ || failed=`expr $failed + 1`
testit "export keytab from domain (2nd time)" $VALGRIND $samba_tool export keytab $PREFIX/tmpkeytab $@ || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpuserccache"
export KRB5CCNAME

testit "kinit with keytab as user" $VALGRIND $samba4kinit --keytab=$PREFIX/tmpkeytab --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpadminccache"
export KRB5CCNAME

testit "kinit with keytab as $USERNAME" $VALGRIND $samba4kinit --keytab=$PREFIX/tmpkeytab --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`

testit "del user" $VALGRIND $samba_tool user delete nettestuser -k yes $@ || failed=`expr $failed + 1`

rm -f $PREFIX/tmpadminccache $PREFIX/tmpuserccache $PREFIX/tmpkeytab
exit $failed
