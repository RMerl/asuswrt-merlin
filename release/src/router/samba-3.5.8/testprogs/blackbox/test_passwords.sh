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
setpassword="$PYTHON `dirname $0`/../../source4/setup/setpassword"
newuser="$PYTHON `dirname $0`/../../source4/setup/newuser"

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

KRB5CCNAME="$PREFIX/tmpuserccache"
export KRB5CCNAME

echo $USERPASS > $PREFIX/tmpuserpassfile

testit "kinit with user password" $samba4kinit --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@02%
testit "change user password with 'net password change' (unforced)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS  -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`

echo $NEWUSERPASS > ./tmpuserpassfile
testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`


USERPASS=$NEWUSERPASS
NEWUSERPASS=testPaSS@03%

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

test_smbclient "Test login with user kerberos (unforced)" 'ls' -k yes -Unettestuser@$REALM%$NEWUSERPASS || failed=`expr $failed + 1`


NEWUSERPASS=testPaSS@04%
testit "set password on user locally" $VALGRIND $setpassword nettestuser --newpassword=$NEWUSERPASS --must-change-at-next-login $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

NEWUSERPASS=testPaSS@05%
testit "change user password with 'net password change' (after must change flag set)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

NEWUSERPASS=testPaSS@06%
testit "set password on user locally" $VALGRIND $setpassword nettestuser --newpassword=$NEWUSERPASS --must-change-at-next-login $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

NEWUSERPASS=testPaSS@07%

cat > ./tmpkpasswdscript <<EOF
expect Password
password ${USERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "change user password with kpasswd (after must change flag set)" $rkpty ./tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

test_smbclient "Test login with user kerberos" 'ls' -k yes -Unettestuser@$REALM%$NEWUSERPASS || failed=`expr $failed + 1`

testit "reset password policies" $VALGRIND $PYTHON ./setup/pwsettings set --configfile=$PREFIX/dc/etc/smb.conf --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=default --max-pwd-age=default || failed=`expr $failed + 1`

NEWUSERPASS=abcdefg
testit_expect_failure "try to set a non-complex password (command should not succeed)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ && failed=`expr $failed + 1`

testit "allow non-complex passwords" $VALGRIND $PYTHON ./setup/pwsettings set --configfile=$PREFIX/dc/etc/smb.conf --complexity=off || failed=`expr $failed + 1`

testit "try to set a non-complex password (command should succeed)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

test_smbclient "test login with non-complex password" 'ls' -k no -Unettestuser@$REALM%$USERPASS || failed=`expr $failed + 1`

NEWUSERPASS=abc
testit_expect_failure "try to set a short password (command should not succeed)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ && failed=`expr $failed + 1`

testit "allow short passwords (length 1)" $VALGRIND $PYTHON ./setup/pwsettings set --configfile=$PREFIX/dc/etc/smb.conf --min-pwd-length=1 || failed=`expr $failed + 1`

testit "try to set a short password (command should succeed)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

testit "require minimum password age of 1 day" $VALGRIND $PYTHON ./setup/pwsettings set --configfile=$PREFIX/dc/etc/smb.conf --min-pwd-age=1 || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@08%
testit_expect_failure "try to change password too quickly (command should not succeed)" $VALGRIND $net password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS -k no $NEWUSERPASS $@ && failed=`expr $failed + 1`

testit "reset password policies" $VALGRIND $PYTHON ./setup/pwsettings set --configfile=$PREFIX/dc/etc/smb.conf --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=default --max-pwd-age=default || failed=`expr $failed + 1`

testit "del user" $VALGRIND $net user delete nettestuser -U"$USERNAME%$PASSWORD" -k no $@ || failed=`expr $failed + 1`

rm -f tmpccfile tmppassfile tmpuserpassfile tmpuserccache tmpkpasswdscript
exit $failed
