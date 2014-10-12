#!/bin/sh

. selftest/test_functions.sh

. selftest/win/wintest_functions.sh

# This variable is defined in the per-hosts .fns file.
. $WINTESTCONF

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_net.sh SERVER USERNAME PASSWORD DOMAIN
EOF
exit 1;
fi

server="$1"
username="$2"
password="$3"
domain="$4"
shift 4

export SMBTORTURE_REMOTE_HOST=$server

base_tests="BASE-UNLINK BASE-ATTR BASE-DELETE BASE-TCON BASE-OPEN BASE-CHKPATH"

all_errs=0
err=0

on_error() {
	errstr=$1

	all_errs=`expr $all_errs + 1`
	restore_snapshot $errstr "$VM_CFG_PATH"
}

for t in $base_tests; do
	test_name="$t / WINDOWS SERVER"
	echo -e "\n$test_name SETUP PHASE"

	setup_share_test

	if [ $err_rtn -ne 0 ]; then
		# If test setup fails, load VM snapshot and skip test.
		on_error "\n$test_name setup failed, skipping test."
	else
		echo -e "\n$test_name setup completed successfully."

		$SMBTORTURE_BIN_PATH -U $username%$password \
			-W $domain //$server/$SMBTORTURE_REMOTE_SHARE_NAME \
			$t || err=1
		if [ $err -ne 0 ]; then
			on_error "\n$test_name failed."
		else
			echo -e "\n$test_name CLEANUP PHASE"
			remove_share_test
			if [ $err_rtn -ne 0 ]; then
				# If cleanup fails, restore VM snapshot.
				on_error "\n$test_name removal failed."
			else
				echo -e "\n$test_name removal completed successfully."
			fi
		fi
	fi
done

exit $all_errs
