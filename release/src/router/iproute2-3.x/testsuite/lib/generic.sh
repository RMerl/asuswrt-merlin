
export DEST="127.0.0.1"

ts_log()
{
	echo "$@"
}

ts_err()
{
	ts_log "$@" | tee >> $ERRF
}

ts_cat()
{
	cat "$@"
}

ts_err_cat()
{
	ts_cat "$@" | tee >> $ERRF
}

ts_tc()
{
	SCRIPT=$1; shift
	DESC=$1; shift
	TMP_ERR=`mktemp /tmp/tc_testsuite.XXXXXX` || exit
	TMP_OUT=`mktemp /tmp/tc_testsuite.XXXXXX` || exit

	$TC $@ 2> $TMP_ERR > $TMP_OUT

	if [ -s $TMP_ERR ]; then
		ts_err "${SCRIPT}: ${DESC} failed:"
		ts_err "command: $TC $@"
		ts_err "stderr output:"
		ts_err_cat $TMP_ERR
		if [ -s $TMP_OUT ]; then
			ts_err "stdout output:"
			ts_err_cat $TMP_OUT
		fi
	elif [ -s $TMP_OUT ]; then
		echo "${SCRIPT}: ${DESC} succeeded with output:"
		cat $TMP_OUT
	else
		echo "${SCRIPT}: ${DESC} succeeded"
	fi

	rm $TMP_ERR $TMP_OUT
}

ts_ip()
{
	SCRIPT=$1; shift
	DESC=$1; shift
	TMP_ERR=`mktemp /tmp/tc_testsuite.XXXXXX` || exit
	TMP_OUT=`mktemp /tmp/tc_testsuite.XXXXXX` || exit

	$IP $@ 2> $TMP_ERR > $TMP_OUT

	if [ -s $TMP_ERR ]; then
		ts_err "${SCRIPT}: ${DESC} failed:"
		ts_err "command: $IP $@"
		ts_err "stderr output:"
		ts_err_cat $TMP_ERR
		if [ -s $TMP_OUT ]; then
			ts_err "stdout output:"
			ts_err_cat $TMP_OUT
		fi
	elif [ -s $TMP_OUT ]; then
		echo "${SCRIPT}: ${DESC} succeeded with output:"
		cat $TMP_OUT
	else
		echo "${SCRIPT}: ${DESC} succeeded"
	fi

	rm $TMP_ERR $TMP_OUT
}

ts_qdisc_available()
{
	HELPOUT=`$TC qdisc add $1 help 2>&1`
	if [ "`echo $HELPOUT | grep \"^Unknown qdisc\"`" ]; then
		return 0;
	else
		return 1;
	fi
}
