#!/bin/sh

# this runs the file serving tests that are expected to pass with samba3

if [ $# != 0 ]; then
cat <<EOF
Usage: test_local_s3.sh
EOF
exit 1;
fi

incdir=`dirname $0`
. $incdir/test_functions.sh

failed=0

testit "talloctort" $VALGRIND $BINDIR/talloctort || \
    failed=`expr $failed + 1`

testit "replacetort" $VALGRIND $BINDIR/replacetort || \
    failed=`expr $failed + 1`

testok $0 $failed
