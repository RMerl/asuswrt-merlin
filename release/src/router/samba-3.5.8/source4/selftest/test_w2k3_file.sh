#!/bin/sh

# this runs the file serving tests that are expected to pass with win2003

if [ $# -lt 3 ]; then
cat <<EOF
Usage: test_w2k3_file.sh UNC USERNAME PASSWORD <first> <smbtorture args>
EOF
exit 1;
fi

unc="$1"
username="$2"
password="$3"
start="$4"
shift 4
ADDARGS="$*"

incdir=`dirname $0`
. $incdir/test_functions.sh

tests="BASE-FDPASS BASE-LOCK "
tests="$tests BASE-UNLINK BASE-ATTR"
tests="$tests BASE-DIR1 BASE-DIR2 BASE-VUID"
tests="$tests BASE-TCON BASE-TCONDEV BASE-RW1"
tests="$tests BASE-DENY3 BASE-XCOPY BASE-OPEN BASE-DENYDOS"
tests="$tests BASE-DELETE BASE-PROPERTIES BASE-MANGLE"
tests="$tests BASE-CHKPATH BASE-SECLEAK BASE-TRANS2"
tests="$tests BASE-NTDENY1 BASE-NTDENY2  BASE-RENAME BASE-OPENATTR"
tests="$tests RAW-QFILEINFO RAW-SFILEINFO-BUG RAW-SFILEINFO"
tests="$tests RAW-LOCK RAW-MKDIR RAW-SEEK RAW-CONTEXT RAW-MUX RAW-OPEN RAW-WRITE"
tests="$tests RAW-UNLINK RAW-READ RAW-CLOSE RAW-IOCTL RAW-CHKPATH RAW-RENAME"
tests="$tests RAW-EAS RAW-STREAMS RAW-OPLOCK RAW-NOTIFY BASE-DELAYWRITE"
# slowest tests last
tests="$tests BASE-DENY1 BASE-DENY2"

# these tests are known to fail against windows
fail="RAW-SEARCH RAW-ACLS RAW-QFSINFO"

echo "Skipping tests expected to fail: $fail"

for t in $tests; do
    testit "$t" smb $VALGRIND bin/smbtorture $TORTURE_OPTIONS $ADDARGS $unc -U"$username"%"$password" $t
done
