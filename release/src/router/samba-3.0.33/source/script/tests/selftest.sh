#!/bin/sh

if [ $# != 3 ]; then
	echo "$0 <directory> <all | quick> <smbtorture4>"
	exit 1
fi

SMBTORTURE4=$3
TESTS=$2

##
## create the test directory
##
PREFIX=`echo $1 | sed s+//+/+`
mkdir -p $PREFIX || exit $?
OLD_PWD=`pwd`
cd $PREFIX || exit $?
PREFIX_ABS=`pwd`
cd $OLD_PWD

if [ -z "$TORTURE_MAXTIME" ]; then
    TORTURE_MAXTIME=300
fi
export TORTURE_MAXTIME

##
## setup the various environment variables we need
##

SERVER=localhost2
SERVER_IP=127.0.0.2
USERNAME=`PATH=/usr/ucb:$PATH whoami`
PASSWORD=test

SRCDIR="`dirname $0`/../.."
BINDIR="`pwd`/bin"
SCRIPTDIR=$SRCDIR/script/tests
SHRDIR=$PREFIX_ABS/tmp
LIBDIR=$PREFIX_ABS/lib
PIDDIR=$PREFIX_ABS/pid
CONFFILE=$LIBDIR/client.conf
SAMBA4CONFFILE=$LIBDIR/samba4client.conf
SERVERCONFFILE=$LIBDIR/server.conf
COMMONCONFFILE=$LIBDIR/common.conf
PRIVATEDIR=$PREFIX_ABS/private
LOCKDIR=$PREFIX_ABS/lockdir
LOGDIR=$PREFIX_ABS/logs
SOCKET_WRAPPER_DIR=$PREFIX/sw
CONFIGURATION="-s $CONFFILE"
SAMBA4CONFIGURATION="-s $SAMBA4CONFFILE"

export PREFIX PREFIX_ABS
export CONFIGURATION CONFFILE SAMBA4CONFIGURATION SAMBA4CONFFILE
export PATH SOCKET_WRAPPER_DIR DOMAIN
export PRIVATEDIR LIBDIR PIDDIR LOCKDIR LOGDIR SERVERCONFFILE
export SRCDIR SCRIPTDIR BINDIR
export USERNAME PASSWORD
export SMBTORTURE4
export SERVER SERVER_IP

PATH=bin:$PATH
export PATH

##
## verify that we were built with --enable-socket-wrapper
##

if test "x`smbd -b | grep SOCKET_WRAPPER`" = "x"; then
	echo "***"
	echo "*** You must include --enable-socket-wrapper when compiling Samba"
	echo "*** in order to execute 'make test'.  Exiting...."
	echo "***"
	exit 1
fi

## 
## create the test directory layout
##
echo -n "CREATE TEST ENVIRONMENT IN '$PREFIX'"...
/bin/rm -rf $PREFIX/*
mkdir -p $PRIVATEDIR $LIBDIR $PIDDIR $LOCKDIR $LOGDIR $SOCKET_WRAPPER_DIR
mkdir -p $PREFIX_ABS/tmp
chmod 777 $PREFIX_ABS/tmp

##
## Create the common config include file with the basic settings
##

cat >$COMMONCONFFILE<<EOF
	workgroup = SAMBA-TEST

	private dir = $PRIVATEDIR
	pid directory = $PIDDIR
	lock directory = $LOCKDIR
	log file = $LOGDIR/log.%m
	log level = 0

	passdb backend = tdbsam

	name resolve order = bcast
EOF

TORTURE_INTERFACES='127.0.0.6/8,127.0.0.7/8,127.0.0.8/8,127.0.0.9/8,127.0.0.10/8,127.0.0.11/8'

cat >$CONFFILE<<EOF
[global]
	netbios name = TORTURE_6
	interfaces = $TORTURE_INTERFACES
	panic action = $SCRIPTDIR/gdb_backtrace %d %\$(MAKE_TEST_BINARY)
	include = $COMMONCONFFILE
EOF

cat >$SAMBA4CONFFILE<<EOF
[global]
	netbios name = TORTURE_6
	interfaces = $TORTURE_INTERFACES
	panic action = $SCRIPTDIR/gdb_backtrace %PID% %PROG%
	include = $COMMONCONFFILE
EOF

cat >$SERVERCONFFILE<<EOF
[global]
	netbios name = $SERVER
	interfaces = $SERVER_IP/8
	bind interfaces only = yes
	panic action = $SCRIPTDIR/gdb_backtrace %d %\$(MAKE_TEST_BINARY)
	include = $COMMONCONFFILE

	; Necessary to add the build farm hacks
	add user script = /bin/false
	add machine script = /bin/false

	kernel oplocks = no

	syslog = no
	printing = bsd
	printcap name = /dev/null

[tmp]
	path = $PREFIX_ABS/tmp
	read only = no
	smbd:sharedelay = 100000
	map hidden = yes
	map system = yes
	create mask = 755
[hideunread]
	copy = tmp
	hide unreadable = yes
[hideunwrite]
	copy = tmp
	hide unwriteable files = yes
[print1]
	copy = tmp
	printable = yes
	printing = test
[print2]
	copy = print1
[print3]
	copy = print1
[print4]
	copy = print1
EOF

##
## create a test account
##

(echo $PASSWORD; echo $PASSWORD) | \
	smbpasswd -c $CONFFILE -L -s -a $USERNAME >/dev/null || exit 1

echo "DONE";

SERVER_TEST_FIFO="$PREFIX/server_test.fifo"
export SERVER_TEST_FIFO
NMBD_TEST_LOG="$PREFIX/nmbd_test.log"
export NMBD_TEST_LOG
SMBD_TEST_LOG="$PREFIX/smbd_test.log"
export SMBD_TEST_LOG

MAKE_TEST_BINARY=""
export MAKE_TEST_BINARY

# start off with 0 failures
failed=0
export failed

. $SCRIPTDIR/test_functions.sh

SOCKET_WRAPPER_DEFAULT_IFACE=2
export SOCKET_WRAPPER_DEFAULT_IFACE
samba3_check_or_start

# ensure any one smbtorture call doesn't run too long
# and smbtorture will use 127.0.0.6 as source address by default
SOCKET_WRAPPER_DEFAULT_IFACE=6
export SOCKET_WRAPPER_DEFAULT_IFACE
TORTURE4_OPTIONS="$SAMBA4CONFIGURATION"
TORTURE4_OPTIONS="$TORTURE4_OPTIONS --maximum-runtime=$TORTURE_MAXTIME"
TORTURE4_OPTIONS="$TORTURE4_OPTIONS --target=samba3"
export TORTURE4_OPTIONS

if [ x"$RUN_FROM_BUILD_FARM" = x"yes" ];then
	TORTURE4_OPTIONS="$TORTURE4_OPTIONS --option=torture:progress=no"
fi


##
## ready to go...now loop through the tests
##

START=`date`
(
 # give time for nbt server to register its names
 echo "delaying for nbt name registration"
 sleep 4
 # This will return quickly when things are up, but be slow if we need to wait for (eg) SSL init 
 bin/nmblookup $CONFIGURATION -U $SERVER_IP __SAMBA__
 bin/nmblookup $CONFIGURATION __SAMBA__
 bin/nmblookup $CONFIGURATION -U 127.255.255.255 __SAMBA__
 bin/nmblookup $CONFIGURATION -U $SERVER_IP $SERVER
 bin/nmblookup $CONFIGURATION $SERVER
 # make sure smbd is also up set
 echo "wait for smbd"
 bin/smbclient $CONFIGURATION -L $SERVER_IP -U% -p 139 | head -2
 bin/smbclient $CONFIGURATION -L $SERVER_IP -U% -p 139 | head -2

 failed=0

 . $SCRIPTDIR/tests_$TESTS.sh
 exit $failed
)
failed=$?

samba3_stop_sig_term

END=`date`
echo "START: $START ($0)";
echo "END:   $END ($0)";

# if there were any valgrind failures, show them
count=`find $PREFIX -name 'valgrind.log*' | wc -l`
if [ "$count" != 0 ]; then
    for f in $PREFIX/valgrind.log*; do
	if [ -s $f ]; then
	    echo "VALGRIND FAILURE";
	    failed=`expr $failed + 1`
	    cat $f
	fi
    done
fi

sleep 2
samba3_stop_sig_kill

teststatus $0 $failed
