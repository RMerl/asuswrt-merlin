#!/bin/sh 

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

rm -rf tests/tmp/db
mkdir -p tests/tmp/db

if [ -f tests/tmp/slapd.pid ]; then
    kill `cat tests/tmp/slapd.pid`
    sleep 1
fi
if [ -f tests/tmp/slapd.pid ]; then
    kill -9 `cat tests/tmp/slapd.pid`
    rm -f tests/tmp/slapd.pid
fi

# we don't consider a slapadd failure as a test suite failure, as it
# has nothing to do with ldb

MODCONF=tests/tmp/modules.conf
rm -f $MODCONF
touch $MODCONF || exit 1

slaptest -u -f $LDBDIR/tests/slapd.conf > /dev/null 2>&1 || {
 echo "enabling sladp modules"
cat > $MODCONF <<EOF
modulepath	/usr/lib/ldap
moduleload	back_bdb
EOF
}

slaptest -u -f $LDBDIR/tests/slapd.conf || {
    echo "slaptest failed - skipping ldap tests"
    exit 0
}

slapadd -f $LDBDIR/tests/slapd.conf < $LDBDIR/tests/init.ldif || exit 0

