local_s3() {
	echo "RUNNING TESTS local_s3"
	$SCRIPTDIR/test_local_s3.sh \
	|| failed=`expr $failed + $?`
}

smbtorture_s3() {
	echo "RUNNING TESTS smbtorture_s3"
	$SCRIPTDIR/test_smbtorture_s3.sh \
		//$SERVER_IP/tmp $USERNAME $PASSWORD "" \
	|| failed=`expr $failed + $?`
}

smbtorture_s3_encrypted() {
	echo "RUNNING TESTS smbtorture_s3_encrypted"
	$SCRIPTDIR/test_smbtorture_s3.sh \
		//$SERVER_IP/tmp $USERNAME $PASSWORD "" "-e" \
	|| failed=`expr $failed + $?`
}

smbclient_s3() {
	echo "RUNNING TESTS smbclient_s3"
	$SCRIPTDIR/test_smbclient_s3.sh $SERVER $SERVER_IP $USERNAME $PASSWORD \
	|| failed=`expr $failed + $?`
}

smbclient_s3_encrypted() {
	echo "RUNNING TESTS smbclient_s3_encrypted"
	$SCRIPTDIR/test_smbclient_s3.sh $SERVER $SERVER_IP $USERNAME $PASSWORD "-e" \
	|| failed=`expr $failed + $?`
}

wbinfo_s3() {
	echo "RUNNING TESTS wbinfo_s3"
	$SCRIPTDIR/test_wbinfo_s3.sh $WORKGROUP $SERVER $USERNAME $PASSWORD \
	|| failed=`expr $failed + $?`
}

ntlm_auth_s3() {
	echo "RUNNING TESTS ntlm_auth_s3"
	$SCRIPTDIR/test_ntlm_auth_s3.sh \
	|| failed=`expr $failed + $?`
}

net_s3() {
	echo "RUNNING TESTS net_s3"
	$SCRIPTDIR/test_net_s3.sh \
	|| failed=`expr $failed + $?`
}

testparm_s3() {
	echo "RUNNING TESTS testparm_s3"
	$SCRIPTDIR/test_testparm_s3.sh \
	|| failed=`expr $failed + $?`
}

posix_s3() {
	echo "RUNNING TESTS posix_s3"
	eval "$LIB_PATH_VAR="\$SAMBA4SHAREDDIR:\$$LIB_PATH_VAR"; export $LIB_PATH_VAR"
	eval echo "$LIB_PATH_VAR=\$$LIB_PATH_VAR"
	if [ -x "$SMBTORTURE4" ]; then
		SMBTORTURE4VERSION=`$SMBTORTURE4 --version`
	fi
	if [ -n "$SMBTORTURE4" -a -n "$SMBTORTURE4VERSION" ];then
		echo "Running Tests with Samba4's smbtorture"
		echo $SMBTORTURE4VERSION
		$SCRIPTDIR/test_posix_s3.sh \
			//$SERVER_IP/tmp $USERNAME $PASSWORD "" \
		|| failed=`expr $failed + $?`
	else
		echo "Skip Tests with Samba4's smbtorture"
		echo "Try to compile with --with-smbtorture4-path=PATH to enable"
	fi
}

failed=0

if test "x$TESTS" = "x" ; then
	local_s3
	smbtorture_s3
	smbtorture_s3_encrypted
	smbclient_s3
	smbclient_s3_encrypted
	wbinfo_s3
	ntlm_auth_s3
	net_s3
	testparm_s3
	posix_s3
else
	for THIS_TEST in $TESTS; do
		$THIS_TEST
	done
fi

