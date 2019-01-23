#!/bin/sh

WBINFO="$VALGRIND ${NET:-$BINDIR/wbinfo} $CONFIGURATION"
TEST_INT=`dirname $0`/test_wbinfo_sids2xids_int.py

incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh

testit "sids2xids" ${TEST_INT} ${WBINFO} || failed=`expr $failed + 1`

testok $0 $failed
