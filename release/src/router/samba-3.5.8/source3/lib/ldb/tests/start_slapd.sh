#!/bin/sh

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

mkdir -p $LDBDIR/tests/tmp/db

# running slapd with -d0 means it stays in the same process group, so it can be
# killed by timelimit
slapd -d0 -f $LDBDIR/tests/slapd.conf -h "`$LDBDIR/tests/ldapi_url.sh`" $* &

sleep 2
