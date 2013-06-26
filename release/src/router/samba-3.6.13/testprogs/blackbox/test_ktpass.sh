#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_newuser.sh PREFIX
EOF
exit 1;
fi

PREFIX="$1"
shift 1

. `dirname $0`/subunit.sh


samba_tool="$BUILDDIR/bin/samba-tool"
samba4bindir="$BUILDDIR/bin"
samba4srcdir="$SRCDIR/source4"
samba4kinit="$samba4bindir/samba4kinit$EXEEXT"
CONFIG="--configfile=$PREFIX/dc/etc/smb.conf"

TESTUSER="ktpassUser"

testit "newuser" $samba_tool newuser $CONFIG $TESTUSER testp@ssw0Rd || failed=`expr $failed + 1`

KRB5CCNAME="$PREFIX/tmpccache"
export KRB5CCNAME
echo "testp@ssw0Rd" >$PREFIX/tmppassfile
testit "kinit with passwd" $samba4kinit -e arcfour-hmac-md5 --password-file=$PREFIX/tmppassfile   $TESTUSER@SAMBA.EXAMPLE.COM   || failed=`expr $failed + 1`
testit "ktpass" $samba4srcdir/scripting/bin/ktpass.sh --host LOCALDC --out $PREFIX/testuser.kt --princ $TESTUSER --pass "testp@ssw0Rd" --path-to-ldbsearch=$BUILDDIR/bin|| failed=`expr $failed + 1`

rm -f $KRB5CCNAME

testit "kinit with keytab" $samba4kinit -e arcfour-hmac-md5 --use-keytab -t $PREFIX/testuser.kt $TESTUSER@SAMBA.EXAMPLE.COM   || failed=`expr $failed + 1`

rm -f $PREFIX/tmpccache $PREFIX/testuser.kt
exit $failed
