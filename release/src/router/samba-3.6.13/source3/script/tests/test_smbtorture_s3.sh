#!/bin/sh

# this runs the file serving tests that are expected to pass with samba3

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_smbtorture_s3.sh TEST UNC USERNAME PASSWORD <smbtorture args>
EOF
exit 1;
fi

t="$1"
unc="$2"
username="$3"
password="$4"
shift 4
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}



failed=0
testit "smbtorture" $VALGRIND $BINDIR/smbtorture $unc -U"$username"%"$password" $ADDARGS $t || failed=`expr $failed + 1`

testok $0 $failed
