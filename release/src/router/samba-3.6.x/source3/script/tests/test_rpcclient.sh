#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: test_rpcclient.sh ccache binding <rpcclient commands>
EOF
exit 1;
fi

KRB5CCNAME=$1
shift 1
export KRB5CCNAME
ADDARGS="$*"

incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
testit "rpcclient" $VALGRIND $BINDIR/rpcclient -c 'getusername' $ADDARGS || failed=`expr $failed + 1`

testok $0 $failed
