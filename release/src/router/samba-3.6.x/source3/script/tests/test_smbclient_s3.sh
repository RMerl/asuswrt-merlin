#!/bin/sh

# this runs the file serving tests that are expected to pass with samba3

if [ $# -lt 7 ]; then
cat <<EOF
Usage: test_smbclient_s3.sh SERVER SERVER_IP USERNAME PASSWORD USERID LOCAL_PATH PREFIX
EOF
exit 1;
fi

SERVER="$1"
SERVER_IP="$2"
USERNAME="$3"
PASSWORD="$4"
USERID="$5"
LOCAL_PATH="$6"
PREFIX="$7"
SMBCLIENT="$VALGRIND ${SMBCLIENT:-$BINDIR/smbclient}"
WBINFO="$VALGRIND ${WBINFO:-$BINDIR/wbinfo}"
shift 7
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}

failed=0

# Test that a noninteractive smbclient does not prompt
test_noninteractive_no_prompt()
{
    prompt="smb"

    cmd='echo du | $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`

    if [ $? != 0 ] ; then
	echo "$out"
	echo "command failed"
	false
	return
    fi

    echo "$out" | grep $prompt >/dev/null 2>&1

    if [ $? = 0 ] ; then
	# got a prompt .. fail
	echo matched interactive prompt in non-interactive mode
	false
    else
	true
    fi
}

# Test that an interactive smbclient prompts to stdout
test_interactive_prompt_stdout()
{
    prompt="smb"
    tmpfile=$PREFIX/smbclient_interactive_prompt_commands

    cat > $tmpfile <<EOF
du
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "command failed"
	false
	return
    fi

    echo "$out" | grep $prompt >/dev/null 2>&1

    if [ $? = 0 ] ; then
	# got a prompt .. succeed
	true
    else
	echo failed to match interactive prompt on stdout
	false
    fi
}

# Test creating a bad symlink and deleting it.
test_bad_symlink()
{
    prompt="posix_unlink deleted file /newname"
    tmpfile=$PREFIX/smbclient_bad_symlinks_commands

    cat > $tmpfile <<EOF
posix
posix_unlink newname
symlink badname newname
posix_unlink newname
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed create then delete bad symlink with error $ret"
	false
	return
    fi

    echo "$out" | grep "$prompt" >/dev/null 2>&1

    ret=$?
    if [ $ret = 0 ] ; then
	# got the correct prompt .. succeed
	true
    else
	echo "$out"
	echo "failed create then delete bad symlink - grep failed with $ret"
	false
    fi
}

# Test creating a good symlink and deleting it by path.
test_good_symlink()
{
    tmpfile=/tmp/smbclient.in.$$
    slink_name="$LOCAL_PATH/slink"
    slink_target="$LOCAL_PATH/slink_target"

    touch $slink_target
    ln -s $slink_target $slink_name
    cat > $tmpfile <<EOF
del slink
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed delete good symlink with error $ret"
	rm $slink_target
	rm $slink_name
	false
	return
    fi

    if [ ! -e $slink_target ] ; then
	echo "failed delete good symlink - symlink target deleted !"
	rm $slink_target
	rm $slink_name
	false
	return
    fi

    if [ -e $slink_name ] ; then
	echo "failed delete good symlink - symlink still exists"
	rm $slink_target
	rm $slink_name
	false
    else
	# got the correct prompt .. succeed
	rm $slink_target
	true
    fi
}

# Test writing into a read-only directory (logon as guest) fails.
test_read_only_dir()
{
    prompt="NT_STATUS_ACCESS_DENIED making remote directory"
    tmpfile=/tmp/smbclient.in.$$

##
## We can't do this as non-root. We always have rights to
## create the directory.
##
    if [ "$USERID" != 0 ] ; then
	echo "skipping test_read_only_dir as non-root"
	true
	return
    fi

##
## We can't do this with an encrypted connection. No credentials
## to set up the channel.
##
    if [ "$ADDARGS" = "-e" ] ; then
	echo "skipping test_read_only_dir with encrypted connection"
	true
	return
    fi

    cat > $tmpfile <<EOF
mkdir a_test_dir
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U% //$SERVER/ro-tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed writing into read-only directory with error $ret"
	false
	return
    fi

    echo "$out" | grep "$prompt" >/dev/null 2>&1

    ret=$?
    if [ $ret = 0 ] ; then
	# got the correct prompt .. succeed
	true
    else
	echo "$out"
	echo "failed writing into read-only directory - grep failed with $ret"
	false
    fi
}

# Test reading an owner-only file (logon as guest) fails.
test_owner_only_file()
{
    prompt="NT_STATUS_ACCESS_DENIED opening remote file"
    tmpfile=/tmp/smbclient.in.$$

##
## We can't do this as non-root. We always have rights to
## read the file.
##
    if [ "$USERID" != 0 ] ; then
	echo "skipping test_owner_only_file as non-root"
	true
	return
    fi

##
## We can't do this with an encrypted connection. No credentials
## to set up the channel.
##
    if [ "$ADDARGS" = "-e" ] ; then
	echo "skipping test_owner_only_file with encrypted connection"
	true
	return
    fi

    cat > $tmpfile <<EOF
get unreadable_file
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U% //$SERVER/ro-tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed reading owner-only file with error $ret"
	false
	return
    fi

    echo "$out" | grep "$prompt" >/dev/null 2>&1

    ret=$?
    if [ $ret = 0 ] ; then
	# got the correct prompt .. succeed
	true
    else
	echo "$out"
	echo "failed reading owner-only file - grep failed with $ret"
	false
    fi
}

# Test accessing an msdfs path.
test_msdfs_link()
{
    tmpfile=/tmp/smbclient.in.$$
    prompt="  msdfs-target  "

    cat > $tmpfile <<EOF
ls
cd \\msdfs-src1
ls msdfs-target
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/msdfs-share -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed accessing \\msdfs-src1 link with error $ret"
	false
	return
    fi

    echo "$out" | grep "$prompt" >/dev/null 2>&1

    ret=$?
    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed listing \\msdfs-src1 - grep failed with $ret"
	false
    fi

    cat > $tmpfile <<EOF
ls
cd \\deeppath\\msdfs-src2
ls msdfs-target
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT "$@" -U$USERNAME%$PASSWORD //$SERVER/msdfs-share -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
    eval echo "$cmd"
    out=`eval $cmd`
    ret=$?
    rm -f $tmpfile

    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed accessing \\deeppath\\msdfs-src2 link with error $ret"
	false
	return
    fi

    echo "$out" | grep "$prompt" >/dev/null 2>&1

    ret=$?
    if [ $ret != 0 ] ; then
	echo "$out"
	echo "failed listing \\deeppath\\msdfs-src2 - grep failed with $ret"
	false
	return
    else
	true
	return
    fi
}

# Test authenticating using the winbind ccache
test_ccache_access()
{
    $WBINFO --ccache-save="${USERNAME}%${PASSWORD}"
    $SMBCLIENT //$SERVER_IP/tmp -C -U "${USERNAME}%" \
	-c quit 2>&1
    ret=$?

    if [ $ret != 0 ] ; then
	echo "smbclient failed to use cached credentials"
	false
	return
    fi

    $WBINFO --ccache-save="${USERNAME}%GarBage"
    $SMBCLIENT //$SERVER_IP/tmp -C -U "${USERNAME}%" \
	-c quit 2>&1
    ret=$?

    if [ $ret -eq 0 ] ; then
	echo "smbclient succeeded with wrong cached credentials"
	false
	return
    fi

    $WBINFO --logoff
}

LOGDIR_PREFIX=test_smbclient_s3

# possibly remove old logdirs:

for OLDDIR in $(find ${PREFIX} -type d -name "${LOGDIR_PREFIX}_*") ;  do
	echo "removing old directory ${OLDDIR}"
	rm -rf ${OLDDIR}
done

LOGDIR=$(mktemp -d ${PREFIX}/${LOGDIR_PREFIX}_XXXXXX)


testit "smbclient -L $SERVER_IP" $SMBCLIENT -L $SERVER_IP -N -p 139 || failed=`expr $failed + 1`
testit "smbclient -L $SERVER -I $SERVER_IP" $SMBCLIENT -L $SERVER -I $SERVER_IP -N -p 139 -c quit || failed=`expr $failed + 1`

testit "noninteractive smbclient does not prompt" \
    test_noninteractive_no_prompt || \
    failed=`expr $failed + 1`

testit "noninteractive smbclient -l does not prompt" \
   test_noninteractive_no_prompt -l $LOGDIR || \
    failed=`expr $failed + 1`

testit "interactive smbclient prompts on stdout" \
   test_interactive_prompt_stdout || \
    failed=`expr $failed + 1`

testit "interactive smbclient -l prompts on stdout" \
   test_interactive_prompt_stdout -l $LOGDIR || \
    failed=`expr $failed + 1`

testit "creating a bad symlink and deleting it" \
   test_bad_symlink || \
   failed=`expr $failed + 1`

testit "creating a good symlink and deleting it by path" \
   test_good_symlink || \
   failed=`expr $failed + 1`

testit "writing into a read-only directory fails" \
   test_read_only_dir || \
   failed=`expr $failed + 1`

testit "Reading a owner-only file fails" \
   test_owner_only_file || \
   failed=`expr $failed + 1`

testit "Accessing an MS-DFS link" \
   test_msdfs_link || \
   failed=`expr $failed + 1`

testit "ccache access works for smbclient" \
    test_ccache_access || \
    failed=`expr $failed + 1`

testit "rm -rf $LOGDIR" \
    rm -rf $LOGDIR || \
    failed=`expr $failed + 1`

testok $0 $failed
