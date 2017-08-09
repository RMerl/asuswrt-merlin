#!/bin/sh

 if [ `whoami` != "root" ]; then
       echo "Windows tests will not run without root privileges."
       exit 1
 fi

 if [ "$DO_SOCKET_WRAPPER" = SOCKET_WRAPPER ]; then
       echo "Windows tests will not run with socket wrapper enabled."
       exit 1
 fi

 if [ ! $WINTESTCONF ]; then
	echo "Environment variable WINTESTCONF has not been defined."
	echo "Windows tests will not run unconfigured."
	exit 1
 fi

 if [ ! -r $WINTESTCONF ]; then
       echo "$WINTESTCONF could not be read."
       exit 1
 fi

 export WINTEST_DIR=$SRCDIR/selftest/win
 export TMPDIR=$TMPDIR
 export NETBIOSNAME=$NETBIOSNAME

 . $WINTESTCONF

 $SRCDIR/selftest/test_win.sh
