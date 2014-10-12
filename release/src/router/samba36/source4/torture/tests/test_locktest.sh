#!/bin/sh
# Blackbox tests for locktest
# Copyright (C) 2008 Andrew Tridgell
# based on test_smbclient.sh

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_locktest.sh SERVER USERNAME PASSWORD DOMAIN PREFIX
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
PASSWORD=$3
DOMAIN=$4
PREFIX=$5
shift 5
failed=0

samba4bindir="$BUILDDIR/bin"
locktest="$samba4bindir/locktest$EXEEXT"

. `dirname $0`/../../../testprogs/blackbox/subunit.sh

testit "locktest" $VALGRIND $locktest //$SERVER/test1 //$SERVER/test2 --num-ops=100  -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@ || failed=`expr $failed + 1`

exit $failed
