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
ENCTYPE=$7
shift 7
failed=0

samba4bindir="$BUILDDIR/bin"
samba4srcdir="$SRCDIR/source4"
smbclient="$samba4bindir/smbclient$EXEEXT"
samba4kinit="$samba4bindir/samba4kinit$EXEEXT"
samba_tool="$samba4bindir/samba-tool$EXEEXT"
ldbmodify="$samba4bindir/ldbmodify$EXEEXT"
ldbsearch="$samba4bindir/ldbsearch$EXEEXT"
rkpty="$samba4bindir/rkpty$EXEEXT"
samba4kpasswd="$samba4bindir/samba4kpasswd$EXEEXT"
enableaccount="$samba_tool enableaccount"
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

PWSETCONFIG="-H ldap://$SERVER -U$USERNAME%$PASSWORD"
export PWSETCONFIG

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME

testit "reset password policies beside of minimum password age of 0 days" $VALGRIND $samba_tool pwsettings $PWSETCONFIG set --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=0 --max-pwd-age=default || failed=`expr $failed + 1`

echo $PASSWORD > $PREFIX/tmppassfile
#testit "kinit with keytab" $samba4kinit $enctype --keytab=$PREFIX/dc/private/secrets.keytab $SERVER\$@$REALM   || failed=`expr $failed + 1`
testit "kinit with password" $samba4kinit $enctype --password-file=$PREFIX/tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit with password (enterprise style)" $samba4kinit $enctype --enterprise --password-file=$PREFIX/tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit with password (windows style)" $samba4kinit $enctype  --renewable --windows --password-file=$PREFIX/tmppassfile --request-pac $USERNAME@$REALM   || failed=`expr $failed + 1`
testit "kinit renew ticket" $samba4kinit $enctype --request-pac -R

test_smbclient "Test login with kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

testit "check time with kerberos ccache" $VALGRIND $samba_tool $CONFIGURATION -k yes $@ time $SERVER || failed=`expr $failed + 1`

USERPASS=testPass@12%
echo $USERPASS > $PREFIX/tmpuserpassfile
testit "add user with kerberos ccache" $VALGRIND $samba_tool user add nettestuser $USERPASS $CONFIGURATION  -k yes $@ || failed=`expr $failed + 1`

echo "Getting defaultNamingContext"
BASEDN=`$ldbsearch $options --basedn='' -H ldap://$SERVER -s base DUMMY=x defaultNamingContext | grep defaultNamingContext | awk '{print $2}'`

cat > $PREFIX/tmpldbmodify <<EOF
dn: cn=nettestuser,cn=users,$BASEDN
changetype: modify
add: servicePrincipalName
servicePrincipalName: host/nettestuser
EOF

testit "modify servicePrincipalName" $VALGRIND $ldbmodify -H ldap://$SERVER $PREFIX/tmpldbmodify -k yes $@ || failed=`expr $failed + 1`

testit "set user password with kerberos ccache" $VALGRIND $samba_tool password set $DOMAIN\\nettestuser $USERPASS $CONFIGURATION  -k yes $@ || failed=`expr $failed + 1`

testit "enable user with kerberos cache" $VALGRIND $enableaccount nettestuser -H ldap://$SERVER -k yes $@ || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpuserccache"
export KRB5CCNAME

testit "kinit with user password" $samba4kinit $enctype --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@34%
testit "change user password with 'samba-tool password change' (rpc)" $VALGRIND $samba_tool password change -W$DOMAIN -U$DOMAIN\\nettestuser%$USERPASS $CONFIGURATION  -k no $NEWUSERPASS $@ || failed=`expr $failed + 1`

echo $NEWUSERPASS > $PREFIX/tmpuserpassfile
testit "kinit with user password" $samba4kinit $enctype --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`


USERPASS=$NEWUSERPASS
NEWUSERPASS=testPaSS@56%
echo $NEWUSERPASS > $PREFIX/tmpuserpassfile

cat > $PREFIX/tmpkpasswdscript <<EOF
expect Password
password ${USERPASS}\n
expect New password
send ${NEWUSERPASS}\n
expect Verify password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "change user password with kpasswd" $rkpty $PREFIX/tmpkpasswdscript $samba4kpasswd nettestuser@$REALM || failed=`expr $failed + 1`

testit "kinit with user password" $samba4kinit $enctype --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@78%
echo $NEWUSERPASS > $PREFIX/tmpuserpassfile

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

cat > $PREFIX/tmpkpasswdscript <<EOF
expect New password
send ${NEWUSERPASS}\n
expect Verify password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "set user password with kpasswd" $rkpty $PREFIX/tmpkpasswdscript $samba4kpasswd --cache=$PREFIX/tmpccache nettestuser@$REALM || failed=`expr $failed + 1`

testit "kinit with user password" $samba4kinit $enctype --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

NEWUSERPASS=testPaSS@910%
echo $NEWUSERPASS > $PREFIX/tmpuserpassfile

cat > $PREFIX/tmpkpasswdscript <<EOF
expect New password
send ${NEWUSERPASS}\n
expect Verify password
send ${NEWUSERPASS}\n
expect Success
EOF

testit "set user password with kpasswd and servicePrincipalName" $rkpty $PREFIX/tmpkpasswdscript $samba4kpasswd --cache=$PREFIX/tmpccache host/nettestuser@$REALM || failed=`expr $failed + 1`

testit "kinit with user password" $samba4kinit $enctype --password-file=$PREFIX/tmpuserpassfile --request-pac nettestuser@$REALM   || failed=`expr $failed + 1`

test_smbclient "Test login with user kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME

lowerrealm=$(echo $REALM | tr '[A-Z]' '[a-z]')
test_smbclient "Test login with user kerberos lowercase realm" 'ls' -k yes -Unettestuser@$lowerrealm%$NEWUSERPASS || failed=`expr $failed + 1`
test_smbclient "Test login with user kerberos lowercase realm 2" 'ls' -k yes -Unettestuser@$REALM%$NEWUSERPASS --realm=$lowerrealm || failed=`expr $failed + 1`

testit "del user with kerberos ccache" $VALGRIND $samba_tool user delete nettestuser $CONFIGURATION -k yes $@ || failed=`expr $failed + 1`

rm -f $KRB5CCNAME
testit "kinit with machineaccountccache script" $machineaccountccache $CONFIGURATION $KRB5CCNAME || failed=`expr $failed + 1`
test_smbclient "Test machine account login with kerberos ccache" 'ls' -k yes || failed=`expr $failed + 1`

testit "reset password policies" $VALGRIND $samba_tool pwsettings $PWSETCONFIG set --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=default --max-pwd-age=default || failed=`expr $failed + 1`

rm -f $PREFIX/tmpccache tmpccfile tmppassfile tmpuserpassfile tmpuserccache tmpkpasswdscript
exit $failed
