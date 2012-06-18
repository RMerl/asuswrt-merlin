#!/bin/sh

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

failed=0

(/usr/bin/env python --version > /dev/null 2>&1)

if test $? -ne 0;
then
	echo "Python binary not found in path. Skipping ntlm_auth tests."
	exit 0
fi

testit "ntlm_auth" $VALGRIND $SRCDIR/torture/test_ntlm_auth.py $BINDIR/ntlm_auth --configfile=$CONFFILE || failed=`expr $failed + 1`
# This should work even with NTLMv2
testit "ntlm_auth" $VALGRIND $SRCDIR/torture/test_ntlm_auth.py $BINDIR/ntlm_auth --configfile=$CONFFILE --client-domain=fOo --server-domain=fOo || failed=`expr $failed + 1`


testok $0 $failed
