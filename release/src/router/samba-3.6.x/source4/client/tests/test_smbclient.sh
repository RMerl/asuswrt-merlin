#!/bin/sh
# Blackbox tests for smbclient
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2007 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_smbclient.sh SERVER USERNAME PASSWORD DOMAIN PREFIX
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
smbclient="$samba4bindir/smbclient$EXEEXT"

. `dirname $0`/../../../testprogs/blackbox/subunit.sh

runcmd() {
	name="$1"
	cmd="$2"
	shift
	shift
	echo "test: $name"
	$VALGRIND $smbclient $CONFIGURATION //$SERVER/tmp -c "$cmd" -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
	fi
	return $status
}

testit "share and server list" $VALGRIND $smbclient -L $SERVER $CONFIGURATION  -W "$DOMAIN" -U"$USERNAME%$PASSWORD" $@ || failed=`expr $failed + 1`

testit "share and server list anonymously" $VALGRIND $smbclient -N -L $SERVER $CONFIGURATION $@ || failed=`expr $failed + 1`

# Use the smbclient binary as our test file
cat $smbclient >tmpfile

# put that file
runcmd "MPutting file" 'mput tmpfile' || failed=`expr $failed + 1`
# check file info
runcmd "Getting alternative name" 'altname tmpfile'|| failed=`expr $failed + 1`
# run allinfo on that file
runcmd "Checking info on file" 'allinfo tmpfile'|| failed=`expr $failed + 1`
# get that file
mv tmpfile tmpfile-old
runcmd "MGetting file" 'mget tmpfile' || failed=`expr $failed + 1`
# remove that file
runcmd "Removing file" 'rm tmpfile' || failed=`expr $failed + 1`
# compare locally
testit "Comparing files" diff tmpfile-old tmpfile || failed=`expr $failed + 1`
# create directory
# cd to directory
# cd to top level directory
# remove directory
runcmd "Creating directory, Changing directory, Going back" 'mkdir bla; cd bla; cd ..; rmdir bla' || failed=`expr $failed + 1`
# enable recurse, create nested directory
runcmd "Creating nested directory" 'mkdir bla/bloe' || failed=`expr $failed + 1`
# remove child directory
runcmd "Removing directory" 'rmdir bla/bloe' || failed=`expr $failed + 1`
# remove parent directory
runcmd "Removing directory" 'rmdir bla'|| failed=`expr $failed + 1`
# enable recurse, create nested directory
runcmd "Creating nested directory" 'mkdir bla' || failed=`expr $failed + 1`
# rename bla to bla2
runcmd "rename of nested directory" 'rename bla bla2' || failed=`expr $failed + 1`
# deltree
runcmd "deltree of nested directory" 'deltree bla2' || failed=`expr $failed + 1`
# run fsinfo
runcmd "Getting file system info" 'fsinfo allocation'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo volume'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo volumeinfo'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo sizeinfo'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo deviceinfo'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo attributeinfo'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo volume-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo size-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo device-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo attribute-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo quota-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo fullsize-information'|| failed=`expr $failed + 1`
runcmd "Getting file system info" 'fsinfo objectid'|| failed=`expr $failed + 1`

# put that file
runcmd "Putting file" 'put tmpfile'|| failed=`expr $failed + 1`
# get that file
mv tmpfile tmpfile-old
runcmd "Getting file" 'get tmpfile'|| failed=`expr $failed + 1`
runcmd "Getting file EA info" 'eainfo tmpfile'|| failed=`expr $failed + 1`
# remove that file
runcmd "Removing file" 'rm tmpfile' || failed=`expr $failed + 1`
# compare locally
testit "Comparing files" diff tmpfile-old tmpfile || failed=`expr $failed + 1`
# put that file
runcmd "Putting file with different name" 'put tmpfile tmpfilex' || failed=`expr $failed + 1`
# get that file
runcmd "Getting file again" 'get tmpfilex' || failed=`expr $failed + 1`
# compare locally
testit "Comparing files" diff tmpfilex tmpfile || failed=`expr $failed + 1`
# remove that file
runcmd "Removing file" 'rm tmpfilex'|| failed=`expr $failed + 1`

runcmd "Lookup name" "lookup $DOMAIN\\$USERNAME" || failed=`expr $failed + 1`

#Fails unless there are privileges
#runcmd "Lookup privs of name" "privileges $DOMAIN\\$USERNAME" || failed=`expr $failed + 1`

# do some simple operations using old protocol versions
runcmd "List directory with LANMAN1" 'ls' -m LANMAN1 --option=clientntlmv2auth=no || failed=`expr $failed + 1`
runcmd "List directory with LANMAN2" 'ls' -m LANMAN2 --option=clientntlmv2auth=no || failed=`expr $failed + 1`

runcmd "Print current working directory" 'pwd'|| failed=`expr $failed + 1`

(
    echo "password=$PASSWORD"
    echo "username=$USERNAME"
    echo "domain=$DOMAIN"
) > tmpauthfile

testit "Test login with --authentication-file" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp --authentication-file=tmpauthfile  || failed=`expr $failed + 1`

PASSWD_FILE="tmppassfile" 
echo "$PASSWORD" > $PASSWD_FILE
export PASSWD_FILE
testit "Test login with PASSWD_FILE" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp -W "$DOMAIN" -U"$USERNAME" || failed=`expr $failed + 1`
PASSWD_FILE=""
export PASSWD_FILE
unset PASSWD_FILE

PASSWD="$PASSWORD" 
export PASSWD
testit "Test login with PASSWD" $VALGRIND $smbclient -c 'ls' $CONFIGURATION //$SERVER/tmp -W "$DOMAIN" -U"$USERNAME" || failed=`expr $failed + 1`

oldUSER=$USER
USER="$USERNAME" 
export USER
testit "Test login with USER and PASSWD" $VALGRIND $smbclient -k no -c 'ls' $CONFIGURATION //$SERVER/tmp -W "$DOMAIN" || failed=`expr $failed + 1`
PASSWD=
export PASSWD
unset PASSWD
USER=$oldUSER
export USER

rm -f tmpfile tmpfile-old tmpfilex tmpauthfile tmppassfile
exit $failed
