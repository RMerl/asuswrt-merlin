#!/bin/sh

# Setup the windows environment.
# This was the best way I could figure out including library files
# for the moment.
# I was finding that "cat common.exp wintest_setup.exp | expect -f -"
# fails to run, but exits with 0 status something like 1% of the time.

setup_share_test()
{
	echo -e "\nSetting up windows environment."
	cat $WINTEST_DIR/common.exp > $TMPDIR/setup.exp
	cat $WINTEST_DIR/wintest_setup.exp >> $TMPDIR/setup.exp
	expect $TMPDIR/setup.exp
	err_rtn=$?
	rm -f $TMPDIR/setup.exp
}

# Clean up the windows environment after the test has run or failed.
remove_share_test()
{
	echo -e "\nCleaning up windows environment."
	cat $WINTEST_DIR/common.exp > $TMPDIR/remove.exp
	cat $WINTEST_DIR/wintest_remove.exp >> $TMPDIR/remove.exp
	expect $TMPDIR/remove.exp
	err_rtn=$?
	rm -f $TMPDIR/remove.exp
}

restore_snapshot()
{
	err_str=$1
	VMX_PATH=$2

	# Display the error that caused us to restore the snapshot.
	echo -e $err_str

	if [ -z $HOST_SERVER_NAME ]; then
		# The vmware server is running locally.
		vmrun revertToSnapshot "$VMX_PATH"
		err_rtn=$?
	else
		vmrun -h $HOST_SERVER_NAME -P $HOST_SERVER_PORT \
			-u $HOST_USERNAME -p $HOST_PASSWORD \
			revertToSnapshot "$VMX_PATH"
		err_rtn=$?
	fi

	if [ $err_rtn -eq 0 ]; then
		echo "Snapshot restored."
	else
		echo "Error $err_rtn restoring snapshot!"
	fi
}
