#!/bin/sh

# this runs the file serving tests that are expected to pass with samba3

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_smbclient_s3.sh SERVER SERVER_IP USERNAME PASSWORD
EOF
exit 1;
fi

SERVER="$1"
SERVER_IP="$2"
USERNAME="$3"
PASSWORD="$4"
SMBCLIENT="$VALGRIND ${SMBCLIENT:-$BINDIR/smbclient} $CONFIGURATION"
shift 4
ADDARGS="$*"

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`
. $incdir/test_functions.sh
}

failed=0

# Test that a noninteractive smbclient does not prompt
test_noninteractive_no_prompt()
{
    prompt="smb"

    cmd='echo du | $SMBCLIENT $CONFIGURATION "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS 2>&1'
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
    tmpfile=/tmp/smbclient.in.$$

    cat > $tmpfile <<EOF
du
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT $CONFIGURATION "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
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
    tmpfile=/tmp/smbclient.in.$$

    cat > $tmpfile <<EOF
posix
posix_unlink newname
symlink badname newname
posix_unlink newname
quit
EOF

    cmd='CLI_FORCE_INTERACTIVE=yes $SMBCLIENT $CONFIGURATION "$@" -U$USERNAME%$PASSWORD //$SERVER/tmp -I $SERVER_IP $ADDARGS < $tmpfile 2>&1'
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


testit "smbclient -L $SERVER_IP" $SMBCLIENT $CONFIGURATION -L $SERVER_IP -N -p 139 || failed=`expr $failed + 1`
testit "smbclient -L $SERVER -I $SERVER_IP" $SMBCLIENT $CONFIGURATION -L $SERVER -I $SERVER_IP -N -p 139 || failed=`expr $failed + 1`

testit "noninteractive smbclient does not prompt" \
    test_noninteractive_no_prompt || \
    failed=`expr $failed + 1`

testit "noninteractive smbclient -l does not prompt" \
   test_noninteractive_no_prompt -l /tmp || \
    failed=`expr $failed + 1`

testit "interactive smbclient prompts on stdout" \
   test_interactive_prompt_stdout || \
    failed=`expr $failed + 1`

testit "interactive smbclient -l prompts on stdout" \
   test_interactive_prompt_stdout -l /tmp || \
    failed=`expr $failed + 1`

testit "creating a bad symlink and deleting it" \
   test_bad_symlink || \
   failed=`expr $failed + 1`

testok $0 $failed
