#!/bin/sh

if [ $# -lt 2 ]; then
cat <<EOF
Usage: test_ntlm_auth_s3.sh PYTHON SRC3DIR
EOF
exit 1;
fi

PYTHON=$1
SRC3DIR=$2
shift 2
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}

failed=0

testit "ntlm_auth" $PYTHON $SRC3DIR/torture/test_ntlm_auth.py $BINDIR/ntlm_auth $ADDARGS || failed=`expr $failed + 1`
# This should work even with NTLMv2
testit "ntlm_auth" $PYTHON $SRC3DIR/torture/test_ntlm_auth.py $BINDIR/ntlm_auth $ADDARGS --client-domain=fOo --server-domain=fOo || failed=`expr $failed + 1`


testok $0 $failed
