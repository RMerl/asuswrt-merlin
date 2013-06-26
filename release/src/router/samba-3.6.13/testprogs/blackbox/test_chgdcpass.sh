#!/bin/sh
# Blackbox tests for kinit and kerberos integration with smbclient etc
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2008 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_kinit.sh SERVER USERNAME REALM DOMAIN PREFIX
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
REALM=$3
DOMAIN=$4
PREFIX=$5
ENCTYPE=$6
PROVDIR=$7
shift 7
failed=0

samba4bindir="$BUILDDIR/bin"
samba4srcdir="$SRCDIR/source4"
smbclient="$samba4bindir/smbclient$EXEEXT"
samba4kinit="$samba4bindir/samba4kinit$EXEEXT"

machineaccountccache="$samba4srcdir/scripting/bin/machineaccountccache"

. `dirname $0`/subunit.sh

test_smbclient() {
	name="$1"
	cmd="$2"
	shift
	shift
	echo "test: $name"
	$VALGRIND $smbclient $CONFIGURATION //$SERVER/tmp -c "$cmd" -W "$DOMAIN" $@
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
	fi
	return $status
}

enctype="-e $ENCTYPE"

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME
rm -f $KRB5CCNAME
testit "kinit with keytab" $samba4kinit $enctype -t $PROVDIR/private/secrets.keytab --use-keytab $USERNAME   || failed=`expr $failed + 1`

#This is important because it puts the ticket for the old KVNO and password into a local ccache
test_smbclient "Test login with kerberos ccache before password change" 'ls' -k yes || failed=`expr $failed + 1`
testit "change dc password" $samba4srcdir/scripting/devel/chgtdcpass -s $PROVDIR/etc/smb.conf || failed=`expr $failed + 1`

#This is important because it shows that the old ticket remains valid (as it must) for incoming connections after the DC password is changed
test_smbclient "Test login with kerberos ccache after password change" 'ls' -k yes || failed=`expr $failed + 1`

#This confirms that the DC password is valid for a kinit too
testit "kinit with keytab" $samba4kinit $enctype -t $PROVDIR/private/secrets.keytab --use-keytab $USERNAME   || failed=`expr $failed + 1`
test_smbclient "Test login with kerberos ccache with fresh kinit" 'ls' -k yes || failed=`expr $failed + 1`
rm -f $KRB5CCNAME

rm -f $PREFIX/tmpccache tmpccfile tmppassfile tmpuserpassfile tmpuserccache tmpkpasswdscript
exit $failed
