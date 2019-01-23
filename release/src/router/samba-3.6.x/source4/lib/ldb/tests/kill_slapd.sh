#!/bin/sh 

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

if [ -f tests/tmp/slapd.pid ]; then
    echo "killing slapd process `cat tests/tmp/slapd.pid`"
    kill -9 `cat tests/tmp/slapd.pid`
    rm -f tests/tmp/slapd.pid
fi
