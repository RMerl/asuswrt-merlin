#!/bin/sh
# Blackbox tests for kinit and kerberos integration with smbclient etc
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2008 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_passwords.sh SERVER USERNAME PASSWORD REALM DOMAIN PREFIX
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
samba_tool="$samba4bindir/samba-tool$EXEEXT"
rkpty="$samba4bindir/rkpty$EXEEXT"
samba4kpasswd="$samba4bindir/samba4kpasswd$EXEEXT"
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

CONFIG="--configfile=$PREFIX/dc/etc/smb.conf"
export CONFIG

testit "reset password policies beside of minimum password age of 0 days" $VALGRIND $samba_tool pwsettings $CONFIG set --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=0 --max-pwd-age=default || failed=`expr $failed + 1`

USERPASS=testPaSS@01%

testit "create user locally" $VALGRIND $newuser $CONFIG nettestuser $USERPASS $@ || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpuserccache"
export KRB5CCNAME

echo $USERPASS > $PREFIX/tmpuserpassfile

testit "kinit with user password" $samba4kinit --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@02%
testit "change user password with 'samba-tool password change' (unforced)" $VALGRIND $samba_tool password change -W$DOMAIN -U$DOMAIN/nettestuser%$USERPASS  -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`

echo $NEWUSERPASS > ./tmpuserpassfile
testit "kinit with user password" $samba4kinit --password-file=./tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`


USERPASS=$NEWUSERPASS
WEAKPASS=testpass1
NEWUSERPASS=testPaSS@03%

# password mismatch check doesn't work yet (kpasswd bug, reported to Love)
#echo "check that password mismatch gives the right error"
#cat > ./tmpkpasswdscript <<EOF
#expect Password
#password ${USERPASS}\n
#expect New password
#send ${WEAKPASS}\n
#expect New password
#send ${NEWUSERPASS}\n
#expect password mismatch
#EOF
#
#testit "change user password with kpasswd" $rkpty ./tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`


echo "check that a weak password is rejected"
cat > ./tmpkpasswdscript <<EOF
expect Password
password ${USERPASS}\n
expect New password
send ${WEAKPASS}\n
expect New password
send ${WEAKPASS}\n
expect Password does not meet complexity requirements
EOF

testit "change to weak user password with kpasswd" $rkpty ./tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`

echo "check that a short password is rejected"
cat > ./tmpkpasswdscript <<EOF
expect Password
password ${USERPASS}\n
expect New password
send xx1\n
expect New password
send xx1\n
expect Password too short
EOF

testit "change to short user password with kpasswd" $rkpty ./tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`


echo "check that a strong new password is accepted"
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
testit "set password on user locally" $VALGRIND $samba_tool setpassword $CONFIG nettestuser --newpassword=$NEWUSERPASS --must-change-at-next-login $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

NEWUSERPASS=testPaSS@05%
testit "change user password with 'samba-tool password change' (after must change flag set)" $VALGRIND $samba_tool password change -W$DOMAIN -U$DOMAIN/nettestuser%$USERPASS -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

NEWUSERPASS=testPaSS@06%
testit "set password on user locally" $VALGRIND $samba_tool setpassword $CONFIG nettestuser --newpassword=$NEWUSERPASS --must-change-at-next-login $@ || failed=`expr $failed + 1`
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

NEWUSERPASS=abcdefg
testit_expect_failure "try to set a non-complex password (command should not succeed)" $VALGRIND $samba_tool password change -W$DOMAIN "-U$DOMAIN/nettestuser%$USERPASS" -k no "$NEWUSERPASS" $@ && failed=`expr $failed + 1`

testit "allow non-complex passwords" $VALGRIND $samba_tool pwsettings set $CONFIG --complexity=off || failed=`expr $failed + 1`

testit "try to set a non-complex password (command should succeed)" $VALGRIND $samba_tool password change -W$DOMAIN "-U$DOMAIN/nettestuser%$USERPASS" -k no "$NEWUSERPASS" $@ || failed=`expr $failed + 1`
USERPASS=$NEWUSERPASS

test_smbclient "test login with non-complex password" 'ls' -k no -Unettestuser@$REALM%$USERPASS || failed=`expr $failed + 1`

NEWUSERPASS=abc
testit_expect_failure "try to set a short password (command should not succeed)" $VALGRIND $samba_tool password change -W$DOMAIN "-U$DOMAIN/nettestuser%$USERPASS" -k no "$NEWUSERPASS" $@ && failed=`expr $failed + 1`

testit "allow short passwords (length 1)" $VALGRIND $samba_tool pwsettings $CONFIG set --min-pwd-length=1 || failed=`expr $failed + 1`

testit "try to set a short password (command should succeed)" $VALGRIND $samba_tool password change -W$DOMAIN "-U$DOMAIN/nettestuser%$USERPASS" -k no "$NEWUSERPASS" $@ || failed=`expr $failed + 1`
USERPASS="$NEWUSERPASS"

testit "require minimum password age of 1 day" $VALGRIND $samba_tool pwsettings $CONFIG set --min-pwd-age=1 || failed=`expr $failed + 1`

testit "show password settings" $VALGRIND $samba_tool pwsettings $CONFIG show || failed=`expr $failed + 1`

NEWUSERPASS="testPaSS@08%"
testit_expect_failure "try to change password too quickly (command should not succeed)" $VALGRIND $samba_tool password change -W$DOMAIN "-U$DOMAIN/nettestuser%$USERPASS" -k no "$NEWUSERPASS" $@ && failed=`expr $failed + 1`

testit "reset password policies" $VALGRIND $samba_tool pwsettings $CONFIG set --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=default --max-pwd-age=default || failed=`expr $failed + 1`

testit "del user" $VALGRIND $samba_tool user delete nettestuser -U"$USERNAME%$PASSWORD" -k no $@ || failed=`expr $failed + 1`

rm -f tmpccfile tmppassfile tmpuserpassfile tmpuserccache tmpkpasswdscript
exit $failed
