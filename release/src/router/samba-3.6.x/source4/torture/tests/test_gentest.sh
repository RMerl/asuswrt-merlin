#!/bin/sh
# Blackbox tests for gentest
# Copyright (C) 2008 Andrew Tridgell
# based on test_smbclient.sh

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_gentest.sh SERVER USERNAME PASSWORD DOMAIN PREFIX
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
gentest="$samba4bindir/gentest$EXEEXT"

. `dirname $0`/../../../testprogs/blackbox/subunit.sh

cat <<EOF > $PREFIX/gentest.ignore
all_info.out.fname
internal_information.out.file_id
EOF

testit "gentest" $VALGRIND $gentest //$SERVER/test1 //$SERVER/test2 --seed=1 --seedsfile=$PREFIX/gentest_seeds.dat --num-ops=100 --ignore=$PREFIX/gentest.ignore -W "$DOMAIN" -U"$USERNAME%$PASSWORD" -U"$USERNAME%$PASSWORD" $@ || failed=`expr $failed + 1`

rm -f $PREFIX/gentest.ignore

exit $failed
