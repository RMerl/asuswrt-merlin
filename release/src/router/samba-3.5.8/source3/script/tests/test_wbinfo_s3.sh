#!/bin/sh

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_wbinfo_s3.sh DOMAIN SERVER USERNAME PASSWORD <wbinfo args>
EOF
exit 1;
fi

domain="$1"
server="$2"
username="$3"
password="$4"
shift 4
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

OLDIFS=$IFS;

tests="--ping"
tests="$tests:--separator"
tests="$tests:--own-domain"
tests="$tests:--all-domains"
tests="$tests:--trusted-domains"
tests="$tests:--domain-info=BUILTIN"
tests="$tests:--domain-info=$domain"
tests="$tests:--online-status"
tests="$tests:--online-status --domain=BUILTIN"
tests="$tests:--online-status --domain=$domain"
#Didn't pass yet# tests="$tests:--domain-users"
tests="$tests:--domain-groups"
tests="$tests:--name-to-sid=$username"
#Didn't pass yet# tests="$tests:--user-info=$username"
tests="$tests:--user-groups=$username"
tests="$tests:--allocate-uid"
tests="$tests:--allocate-gid"

failed=0

OLDIFS=$IFS
NEWIFS=$':'
IFS=$NEWIFS
for t in $tests; do
   IFS=$OLDIFS
   testit "wbinfo $t" $VALGRIND $BINDIR/wbinfo $ADDARGS $t || failed=`expr $failed + 1`
   IFS=$NEWIFS
done
IFS=$OLDIFS

testok $0 $failed
