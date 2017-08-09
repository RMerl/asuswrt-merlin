#!/bin/sh

incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh

TESTPROG=$BINDIR/pthreadpooltest

if [ ! -x $BINDIR/pthreadpooltest ] ; then
    TESTPROG=/bin/true
fi

failed=0

testit "pthreadpool" $VALGRIND $TESTPROG ||
	failed=`expr $failed + 1`

testok $0 $failed
