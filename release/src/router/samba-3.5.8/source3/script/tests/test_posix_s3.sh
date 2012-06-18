#!/bin/sh

# this runs the file serving tests that are expected to pass with samba3

if [ $# -lt 3 ]; then
cat <<EOF
Usage: test_posix_s3.sh UNC USERNAME PASSWORD <first> <smbtorture args>
EOF
exit 1;
fi

unc="$1"
username="$2"
password="$3"
start="$4"
shift 4
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

base="BASE-ATTR BASE-CHARSET BASE-CHKPATH BASE-DEFER_OPEN BASE-DELAYWRITE BASE-DELETE"
base="$base BASE-DENY1 BASE-DENY2 BASE-DENY3 BASE-DENYDOS BASE-DIR1 BASE-DIR2"
base="$base BASE-DISCONNECT BASE-FDPASS BASE-LOCK"
base="$base BASE-MANGLE BASE-NEGNOWAIT BASE-NTDENY1"
base="$base BASE-NTDENY2 BASE-OPEN BASE-OPENATTR BASE-PROPERTIES BASE-RENAME BASE-RW1"
base="$base BASE-SECLEAK BASE-TCON BASE-TCONDEV BASE-TRANS2 BASE-UNLINK BASE-VUID"
base="$base BASE-XCOPY BASE-SAMBA3ERROR"

raw="RAW-ACLS RAW-CHKPATH RAW-CLOSE RAW-COMPOSITE RAW-CONTEXT RAW-EAS"
raw="$raw RAW-IOCTL RAW-LOCK RAW-MKDIR RAW-MUX RAW-NOTIFY RAW-OPEN RAW-OPLOCK"
raw="$raw RAW-QFILEINFO RAW-QFSINFO RAW-READ RAW-RENAME RAW-SEARCH RAW-SEEK"
raw="$raw RAW-SFILEINFO RAW-SFILEINFO-BUG RAW-STREAMS RAW-UNLINK RAW-WRITE"
raw="$raw RAW-SAMBA3HIDE RAW-SAMBA3BADPATH RAW-SFILEINFO-RENAME"
raw="$raw RAW-SAMBA3CASEINSENSITIVE RAW-SAMBA3POSIXTIMEDLOCK"
raw="$raw RAW-SAMBA3ROOTDIRFID"

rpc="RPC-AUTHCONTEXT RPC-SAMBA3-BIND RPC-SAMBA3-SRVSVC RPC-SAMBA3-SHARESEC"
rpc="$rpc RPC-SAMBA3-SPOOLSS RPC-SAMBA3-WKSSVC RPC-SAMBA3-WINREG"
rpc="$rpc RPC-SAMBA3-NETLOGON RPC-SAMBA3-SESSIONKEY RPC-SAMBA3-GETUSERNAME"
rpc="$rpc RPC-SVCCTL RPC-SPOOLSS RPC-SPOOLSS-WIN RPC-NTSVCS RPC-WINREG"
rpc="$rpc RPC-LSA-GETUSER RPC-LSA-LOOKUPSIDS RPC-LSA-LOOKUPNAMES"
rpc="$rpc RPC-LSA-PRIVILEGES "
rpc="$rpc RPC-SAMR RPC-SAMR-USERS RPC-SAMR-USERS-PRIVILEGES RPC-SAMR-PASSWORDS"
rpc="$rpc RPC-SAMR-PASSWORDS-PWDLASTSET RPC-SAMR-LARGE-DC RPC-SAMR-MACHINE-AUTH"
rpc="$rpc RPC-NETLOGON-S3 RPC-NETLOGON-ADMIN"
rpc="$rpc RPC-SCHANNEL RPC-SCHANNEL2 RPC-BENCH-SCHANNEL1 RPC-JOIN"

local="LOCAL-NSS-WRAPPER LOCAL-NDR"

winbind="WINBIND-WBCLIENT"

# NOTE: to enable the UNIX-WHOAMI test, we need to change the default share
# config to allow guest access. I'm not sure whether this would break other
# tests, so leaving it alone for now -- jpeach
unix="UNIX-INFO2"

tests="$base $raw $rpc $unix $local $winbind"

if test "x$POSIX_SUBTESTS" != "x" ; then
	tests="$POSIX_SUBTESTS"
fi

skipped="BASE-CHARSET BASE-TCONDEV"
skipped="$skipped RAW-ACLS RAW-COMPOSITE RAW-CONTEXT"
skipped="$skipped RAW-IOCTL"
skipped="$skipped RAW-QFILEINFO RAW-QFSINFO"
skipped="$skipped RAW-SFILEINFO"

echo "WARNING: Skipping tests $skipped"

ADDARGS="$ADDARGS --option=torture:sharedelay=100000"
#ADDARGS="$ADDARGS --option=torture:writetimeupdatedelay=500000"

failed=0
for t in $tests; do
    if [ ! -z "$start" -a "$start" != $t ]; then
	continue;
    fi
    skip=0
    for s in $skipped; do
    	if [ x"$s" = x"$t" ]; then
    	    skip=1;
	    break;
	fi
    done
    if [ $skip = 1 ]; then
    	continue;
    fi
    start=""
    name="$t"
    if [ "$t" = "BASE-DELAYWRITE" ]; then
	    testit "$name" $VALGRIND $SMBTORTURE4 $TORTURE4_OPTIONS --maximum-runtime=900 $ADDARGS $unc -U"$username"%"$password" $t || failed=`expr $failed + 1`
    else
	    testit "$name" $VALGRIND $SMBTORTURE4 $TORTURE4_OPTIONS $ADDARGS $unc -U"$username"%"$password" $t || failed=`expr $failed + 1`
    fi
    if [ "$t" = "RAW-CHKPATH" ]; then
	    echo "Testing with case sensitive"
	    testit "$name" $VALGRIND $SMBTORTURE4 $TORTURE4_OPTIONS $ADDARGS "$unc"case -U"$username"%"$password" $t || failed=`expr $failed + 1`
    fi
done

testok $0 $failed
