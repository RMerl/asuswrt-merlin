#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: test_wbinfo_s3.sh <wbinfo args>
EOF
exit 1;
fi

ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}

testit "wbinfo" $VALGRIND $BINDIR/wbinfo $ADDARGS || failed=`expr $failed + 1`

testok $0 $failed
