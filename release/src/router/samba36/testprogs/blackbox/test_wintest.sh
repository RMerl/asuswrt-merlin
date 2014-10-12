#!/bin/sh
# Blackbox tests for testing against windows machines
# Copyright (C) 2008 Jim McDonough


testwithconf() {
# define test variables, startup/shutdown scripts
. $1
shift 1

if [ -n "$WINTEST_STARTUP" ]; then
. $WINTEST_STARTUP;
fi

testit "smbtorture" $smbtorture //$SERVER/$SHARE RAW-OPEN -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@ || failed=`expr $failed + 1`

if [ -n "$WINTEST_SHUTDOWN" ]; then
. $WINTEST_SHUTDOWN;
fi
}


# main
# skip without WINTEST_CONF_DIR
if [ -z "$WINTEST_CONF_DIR" ]; then
exit 0;
fi

SOCKET_WRAPPER_DIR=
export -n SOCKET_WRAPPER_DIR

failed=0

$basedir=`pwd`

samba4bindir=`dirname $0`/../../source4/bin
smbtorture=$samba4bindir/smbtorture

. `dirname $0`/subunit.sh

for wintest_conf in $WINTEST_CONF_DIR/*.conf; do
testwithconf "$wintest_conf" $@;
done

exit $failed
