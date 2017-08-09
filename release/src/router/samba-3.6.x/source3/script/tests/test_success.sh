#!/bin/sh
#
# Blackbox test that should simply succeed.
#
# Copyright (C) 2011 Michael Adam <obnox@samba.org>

# include the blackbox subunit infrastructure
# if not run from classical s3 test script:
test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
	incdir=`dirname $0`/../../../testprogs/blackbox
	. $incdir/subunit.sh
}

failed=0

test_success()
{
	true
}

testit "success" \
	test_success || \
	failed=`expr $failed + 1`

testok $0 $failed

