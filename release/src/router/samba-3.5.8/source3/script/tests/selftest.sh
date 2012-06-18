#!/bin/sh

if [ $# -lt 2 ]; then
	echo "$0 <directory> <all | quick> [-t <smbtorture4>] [-s <shrdir>] " \
	     "[-c <custom conf>]"
	exit 1
fi

##
## Setup the required args
##
DIRECTORY=$1; shift;
SUBTESTS=$1; shift;

##
## Parse oprtional args
##
while getopts s:c:t: f
do
    case $f in
	t)	SMBTORTURE4=$OPTARG;;
	s)	ALT_SHRDIR_ARG=$OPTARG;;
	c)      CUSTOM_CONF_ARG=$OPTARG;;
    esac
done

echo "Running selftest with the following"
echo "Selftest Directory: $DIRECTORY"
echo "Subtests to Run: $SUBTESTS"
echo "smbtorture4 Path: $SMBTORTURE4"
echo "Alternative Share Dir: $ALT_SHRDIR_ARG"
echo "Custom Configuration: $CUSTOM_CONF_ARG"

if [ $CUSTOM_CONF_ARG ]; then
    INCLUDE_CUSTOM_CONF="include = $CUSTOM_CONF_ARG"
fi

##
## create the test directory
##
PREFIX=`echo $DIRECTORY | sed s+//+/+`
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

WORKGROUP=SAMBA-TEST
SERVER=localhost2
SERVER_IP=127.0.0.2
if [ ! "x$USER" = "x" ]; then
    USERNAME=$USER
else
    if [ ! "x$LOGNAME" = "x" ]; then
        USERNAME=$LOGNAME
    else
        USERNAME=`PATH=/usr/ucb:$PATH whoami || id -un`
    fi
fi
USERID=`PATH=/usr/ucb:$PATH id | cut -d ' ' -f1 | sed -e 's/uid=\([0-9]*\).*/\1/g'`
GROUPID=`PATH=/usr/ucb:$PATH id | cut -d ' ' -f2 | sed -e 's/gid=\([0-9]*\).*/\1/g'`
PASSWORD=test

SRCDIR="`dirname $0`/../.."
BINDIR="`pwd`/bin"
SCRIPTDIR=$SRCDIR/script/tests
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
CONFIGURATION="--configfile $CONFFILE"
SAMBA4CONFIGURATION="-s $SAMBA4CONFFILE"
NSS_WRAPPER_PASSWD="$PRIVATEDIR/passwd"
NSS_WRAPPER_GROUP="$PRIVATEDIR/group"
WINBINDD_SOCKET_DIR=$PREFIX_ABS/winbindd
WINBINDD_PRIV_PIPE_DIR=$LOCKDIR/winbindd_privileged
TEST_DIRECTORY=$DIRECTORY

export PREFIX PREFIX_ABS
export CONFIGURATION CONFFILE SAMBA4CONFIGURATION SAMBA4CONFFILE
export PATH SOCKET_WRAPPER_DIR DOMAIN
export PRIVATEDIR LIBDIR PIDDIR LOCKDIR LOGDIR SERVERCONFFILE
export SRCDIR SCRIPTDIR BINDIR
export USERNAME PASSWORD
export WORKGROUP SERVER SERVER_IP
export NSS_WRAPPER_PASSWD NSS_WRAPPER_GROUP
export WINBINDD_SOCKET_DIR WINBINDD_PRIV_PIPE_DIR
export TEST_DIRECTORY

PATH=bin:$PATH
export PATH

if [ $SMBTORTURE4 ]; then
    SAMBA4BINDIR=`dirname $SMBTORTURE4`
fi

SAMBA4SHAREDDIR="$SAMBA4BINDIR/shared"

export SAMBA4SHAREDDIR
export SMBTORTURE4

if [ -z "$LIB_PATH_VAR" ] ; then
	echo "Warning: LIB_PATH_VAR not set. Using best guess LD_LIBRARY_PATH." >&2
	LIB_PATH_VAR=LD_LIBRARY_PATH
	export LIB_PATH_VAR
fi

eval $LIB_PATH_VAR=$BINDIR:$SAMBA4SHAREDDIR:\$$LIB_PATH_VAR
export $LIB_PATH_VAR

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

if test "x`smbd -b | grep NSS_WRAPPER`" = "x"; then
	echo "***"
	echo "*** You must include --enable-nss-wrapper when compiling Samba"
	echo "*** in order to execute 'make test'.  Exiting...."
	echo "***"
	exit 1
fi


## 
## create the test directory layout
##
printf "%s" "CREATE TEST ENVIRONMENT IN '$PREFIX'"...
/bin/rm -rf $PREFIX/*
mkdir -p $PRIVATEDIR $LIBDIR $PIDDIR $LOCKDIR $LOGDIR
mkdir -p $SOCKET_WRAPPER_DIR
mkdir -p $WINBINDD_SOCKET_DIR
chmod 755 $WINBINDD_SOCKET_DIR

##
## Create an alternate shrdir if one was specified.
##
if [ $ALT_SHRDIR_ARG ]; then
    ALT_SHRDIR=`echo $ALT_SHRDIR_ARG | sed s+//+/+`
    mkdir -p $ALT_SHRDIR || exit $?
    OLD_PWD=`pwd`
    cd $ALT_SHRDIR || exit $?
    SHRDIR=`pwd`
    cd $OLD_PWD
    /bin/rm -rf $SHRDIR/*
else
    SHRDIR=$PREFIX_ABS/tmp
    mkdir -p $SHRDIR
fi
chmod 777 $SHRDIR

##
## Create the common config include file with the basic settings
##

cat >$COMMONCONFFILE<<EOF
	workgroup = $WORKGROUP

	private dir = $PRIVATEDIR
	pid directory = $PIDDIR
	lock directory = $LOCKDIR
	log file = $LOGDIR/log.%m
	log level = 0

	name resolve order = bcast
EOF

TORTURE_INTERFACES='127.0.0.6/8,127.0.0.7/8,127.0.0.8/8,127.0.0.9/8,127.0.0.10/8,127.0.0.11/8'

cat >$CONFFILE<<EOF
[global]
	netbios name = TORTURE_6
	interfaces = $TORTURE_INTERFACES
	panic action = $SCRIPTDIR/gdb_backtrace %d %\$(MAKE_TEST_BINARY)
	include = $COMMONCONFFILE

	passdb backend = tdbsam
EOF

cat >$SAMBA4CONFFILE<<EOF
[global]
	netbios name = TORTURE_6
	interfaces = $TORTURE_INTERFACES
	panic action = $SCRIPTDIR/gdb_backtrace %PID% %PROG%
	include = $COMMONCONFFILE
	modules dir = $SRCDIR/bin/modules
EOF

cat >$SERVERCONFFILE<<EOF
[global]
	netbios name = $SERVER
	interfaces = $SERVER_IP/8
	bind interfaces only = yes
	panic action = $SCRIPTDIR/gdb_backtrace %d %\$(MAKE_TEST_BINARY)
	include = $COMMONCONFFILE

	state directory = $LOCKDIR
	cache directory = $LOCKDIR

	passdb backend = tdbsam

	domain master = yes
	domain logons = yes
	lanman auth = yes
	time server = yes

	add user script =		$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --passwd_path $NSS_WRAPPER_PASSWD --type passwd --action add --name %u
	add group script =		$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --group_path  $NSS_WRAPPER_GROUP  --type group  --action add --name %g
	add user to group script =	$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --group_path  $NSS_WRAPPER_GROUP  --type member --action add --name %g --member %u --passwd_path $NSS_WRAPPER_PASSWD
	add machine script =		$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --passwd_path $NSS_WRAPPER_PASSWD --type passwd --action add --name %u
	delete user script =		$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --passwd_path $NSS_WRAPPER_PASSWD --type passwd --action delete --name %u
	delete group script =		$PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --group_path  $NSS_WRAPPER_GROUP  --type group  --action delete --name %g
	delete user from group script = $PERL $SRCDIR/../lib/nss_wrapper/nss_wrapper.pl --group_path  $NSS_WRAPPER_GROUP  --type member --action delete --name %g --member %u --passwd_path $NSS_WRAPPER_PASSWD

	kernel oplocks = no
	kernel change notify = no

	syslog = no
	printing = bsd
	printcap name = /dev/null

	winbindd:socket dir = $WINBINDD_SOCKET_DIR
	idmap uid = 100000-200000
	idmap gid = 100000-200000

#	min receivefile size = 4000

	read only = no
	smbd:sharedelay = 100000
#	smbd:writetimeupdatedelay = 500000
	map hidden = no
	map system = no
	map readonly = no
	store dos attributes = yes
	create mask = 755
	store create time = yes
	vfs objects = $BINDIR/xattr_tdb.so $BINDIR/streams_depot.so

	#Include user defined custom parameters if set
	$INCLUDE_CUSTOM_CONF

[tmp]
	path = $SHRDIR
[hideunread]
	copy = tmp
	hide unreadable = yes
[tmpcase]
	copy = tmp
	case sensitive = yes
[hideunwrite]
	copy = tmp
	hide unwriteable files = yes
[print1]
	copy = tmp
	printable = yes
	printing = vlp
	print command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb print %p %s
	lpq command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb lpq %p
	lp rm command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb lprm %p %j
	lp pause command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb lppause %p %j
	lp resume command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb lpresume %p %j
	queue pause command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb queuepause %p
	queue resume command = $BINDIR/vlp tdbfile=$LOCKDIR/vlp.tdb queueresume %p

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

cat >$NSS_WRAPPER_PASSWD<<EOF
root:x:65533:65532:root gecos:$PREFIX_ABS:/bin/false
nobody:x:65534:65533:nobody gecos:$PREFIX_ABS:/bin/false
$USERNAME:x:$USERID:$GROUPID:$USERNAME gecos:$PREFIX_ABS:/bin/false
EOF

cat >$NSS_WRAPPER_GROUP<<EOF
nobody:x:65533:
nogroup:x:65534:nobody
root:x:65532:
$USERNAME-group:x:$GROUPID:
EOF

MAKE_TEST_BINARY="bin/smbpasswd"
export MAKE_TEST_BINARY

(echo $PASSWORD; echo $PASSWORD) | \
	bin/smbpasswd -c $SERVERCONFFILE -L -s -a $USERNAME >/dev/null || exit 1

echo "DONE";

MAKE_TEST_BINARY=""

SERVER_TEST_FIFO="$PREFIX/server_test.fifo"
export SERVER_TEST_FIFO
NMBD_TEST_LOG="$PREFIX/nmbd_test.log"
export NMBD_TEST_LOG
WINBINDD_TEST_LOG="$PREFIX/winbindd_test.log"
export WINBINDD_TEST_LOG
SMBD_TEST_LOG="$PREFIX/smbd_test.log"
export SMBD_TEST_LOG

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
TORTURE4_OPTIONS="$TORTURE4_OPTIONS --option=torture:localdir=$SHRDIR"
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
 sleep 10
 # This will return quickly when things are up, but be slow if we need to wait for (eg) SSL init 
 MAKE_TEST_BINARY="bin/nmblookup"
 bin/nmblookup $CONFIGURATION -U $SERVER_IP __SAMBA__
 bin/nmblookup $CONFIGURATION __SAMBA__
 bin/nmblookup $CONFIGURATION -U 127.255.255.255 __SAMBA__
 bin/nmblookup $CONFIGURATION -U $SERVER_IP $SERVER
 bin/nmblookup $CONFIGURATION $SERVER
 # make sure smbd is also up set
 echo "wait for smbd"
 MAKE_TEST_BINARY="bin/smbclient"
 bin/smbclient $CONFIGURATION -L $SERVER_IP -U% -p 139 | head -2
 bin/smbclient $CONFIGURATION -L $SERVER_IP -U% -p 139 | head -2
 MAKE_TEST_BINARY=""

 failed=0

 . $SCRIPTDIR/tests_$SUBTESTS.sh
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
