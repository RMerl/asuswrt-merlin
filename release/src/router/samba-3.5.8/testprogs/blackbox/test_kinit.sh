#!/bin/sh
# Blackbox tests for kinit and kerberos integration with smbclient etc
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2008 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_kinit.sh SERVER USERNAME PASSWORD REALM DOMAIN PREFIX
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
PASSWORD=$3
REALM=$4
DOMAIN=$5
PREFIX=$6
shift 6
failed=0

samba4bindir="$BUILDDIR/bin"
smbclient="$samba4bindir/smbclient$EXEEXT"
samba4kinit="$samba4bindir/samba4kinit$EXEEXT"
net="$samba4bindir/net$EXEEXT"
rkpty="$samba4bindir/rkpty$EXEEXT"
samba4kpasswd="$samba4bindir/samba4kpasswd$EXEEXT"
enableaccount="$PYTHON `dirname $0`/../../source4/setup/enableaccount"

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

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME

echo $PASSWORD > ./tmppassfile
#testit "kinit with keytab" $samba4kinit --keytab=$PREFIX/dc/private/secrets.keytab $SERVER\$@$REALM   || failed=`expr $failed + 1`
testit "kinit with password" $samba4kinit --password-file=./tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit with password (enterprise style)" $samba4kinit --enterprise --password-file=./tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit with password (windows style)" $samba4kinit --windows --password-file=./tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit with pkinit (name specified)" $samba4kinit --request-pac --renewable --pk-user=FILE:$PREFIX/dc/private/tls/admincert.pem,$PREFIX/dc/private/tls/adminkey.pem $USERNAME@$REALM || failed=`expr $failed + 1`
testit "kinit with pkinit (enterprise name specified)" $samba4kinit --request-pac --renewable --pk-user=FILE:$PREFIX/dc/private/tls/admincert.pem,$PREFIX/dc/private/tls/adminkey.pem --enterprise $USERNAME@$REALM || failed=`expr $failed + 1`
testit "kinit with pkinit (enterprise name in cert)" $samba4kinit --request-pac --renewable --pk-user=FILE:$PREFIX/dc/private/tls/admincertupn.pem,$PREFIX/dc/private/tls/adminkey.pem --pk-enterprise || failed=`expr $failed + 1`
testit "kinit renew ticket" $samba4kinit --request-pac -R

test_smbclient "Test login with kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

testit "domain join with kerberos ccache" $VALGRIND $net join $DOMAIN $CONFIGURATION  -W "$DOMAIN" -k yes $@ || failed=`expr $failed + 1`
testit "check time with kerberos ccache" $VALGRIND $net time $SERVER $CONFIGURATION  -W "$DOMAIN" -k yes $@ || failed=`expr $failed + 1`

testit "add user with kerberos ccache" $VALGRIND $net user add nettestuser $CONFIGURATION  -k yes $@ || failed=`expr $failed + 1`
USERPASS=testPass@12%
echo $USERPASS > ./tmpuserpassfile

testit "set user password with kerberos ccache" $VALGRIND $net password set $DOMAIN\\nettestuser $USERPASS $CONFIGURATION  -k yes $@ || failed=`expr $failed + 1`

testit "enable user with kerberos cache" $VALGRIND $enableaccount nettestuser -H ldap://$SERVER -k yes $@ || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpuserccache"
export KRB5CCNAME

testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@34%
testit "change user password with 'net password change' (rpc)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS $CONFIGURATION  -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`

echo $NEWUSERPASS > ./tmpuserpassfile
testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`


USERPASS=$NEWUSERPASS
NEWUSERPASS=testPaSS@56%
echo $NEWUSERPASS > ./tmpuserpassfile

cat > ./tmpkpasswdscript <<EOF
expect Password
password ${USERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "change user password with kpasswd" $rkpty ./tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`

testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@78%
echo $NEWUSERPASS > ./tmpuserpassfile

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

cat > ./tmpkpasswdscript <<EOF
expect New password
send ${NEWUSERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "set user password with kpasswd" $rkpty ./tmpkpasswdscript $samba4kpasswd --cache=$PREFIX/tmpccache nettestuser@$REALM || failed=`expr $failed + 1`

testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME

testit "del user with kerberos ccache" $VALGRIND $net user delete nettestuser $CONFIGURATION -k yes $@ || failed=`expr $failed + 1`

rm -f tmpccfile tmppassfile tmpuserpassfile tmpuserccache tmpkpasswdscript
exit $failed
