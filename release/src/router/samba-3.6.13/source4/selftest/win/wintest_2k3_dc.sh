#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: wintest_2k3_dc.sh TESTGROUP
EOF
exit 1;
fi

TESTGROUP=$1

if [ -z $WINTEST_DIR ]; then
	echo "Environment variable WINTEST_DIR not found."
	exit 1;
fi

# This variable is defined in the per-hosts .fns file for build-farm hosts that run windows tests.
if [ -z $WINTESTCONF ]; then
	echo "Please point environment variable WINTESTCONF to your test_win.conf file."
	exit 1;
fi

. $WINTESTCONF
. $WINTEST_DIR/wintest_functions.sh

export WIN2K3_DC_REMOTE_HOST=`perl -I$WINTEST_DIR $WINTEST_DIR/vm_get_ip.pl WIN2K3_DC_VM_CFG_PATH`

if [ -z $WIN2K3_DC_REMOTE_HOST ]; then
	# Restore snapshot to ensure VM is in a known state, then exit.
	restore_snapshot "Test failed to get the IP address of the windows 2003 DC." "$WIN2K3_DC_VM_CFG_PATH"
	exit 1;
fi

server=$WIN2K3_DC_REMOTE_HOST
username=$WIN2K3_DC_USERNAME
password=$WIN2K3_DC_PASSWORD
domain=$WIN2K3_DC_DOMAIN
realm=$WIN2K3_DC_REALM

OPTIONS="-U$username%$password -W $domain --option realm=$realm"

all_errs=0

on_error() {
	name=$1

	all_errs=`expr $all_errs + 1`
	restore_snapshot "$name test failed." "$WIN2K3_DC_VM_CFG_PATH"
}

drsuapi_tests() {

	name="RPC-DRSUAPI on ncacn_ip_tcp with seal"
	bin/smbtorture \
		ncacn_ip_tcp:$server[seal] $OPTIONS \
		RPC-DRSUAPI || on_error "$name"

	name="RPC-DRSUAPI on ncacn_ip_tcp with seal,bigendian"
	bin/smbtorture \
		ncacn_ip_tcp:$server[seal,bigendian] $OPTIONS \
		RPC-DRSUAPI || on_error "$name"
}

spoolss_tests() {

	name="RPC-SPOOLSS on ncacn_np"
	bin/smbtorture \
		ncacn_np:$server $OPTIONS \
		RPC-SPOOLSS || on_error "$name"
}

ncacn_ip_tcp_tests() {
	bindopt=$1
	transport="ncacn_ip_tcp"
	tests="RPC-SCHANNEL RPC-EPMAPPER RPC-SAMR RPC-NETLOGON RPC-LSA RPC-SAMLOGON RPC-SAMSYNC RPC-MULTIBIND"

	for bindoptions in $bindopt; do
		for t in $tests; do
			name="$t on $transport with $bindoptions"
			bin/smbtorture $TORTURE_OPTIONS \
				$transport:$server[$bindoptions] \
				$OPTIONS $t || on_error "$name"
		done
	done
}

ncacn_np_tests() {
	bindopt=$1
	transport="ncacn_np"
	tests="RPC-SCHANNEL RPC-DSSETUP RPC-EPMAPPER RPC-SAMR RPC-WKSSVC RPC-SRVSVC RPC-EVENTLOG RPC-NETLOGON RPC-LSA RPC-SAMLOGON RPC-SAMSYNC RPC-MULTIBIND RPC-WINREG"

	for bindoptions in $bindopt; do
		for t in $tests; do
			name="$t on $transport with $bindoptions"
			bin/smbtorture $TORTURE_OPTIONS \
				$transport:$server[$bindoptions] \
				$OPTIONS $t || on_error "$name"
		done
	done
}

bindoptions="padcheck connect sign seal ntlm,sign ntml,seal $VALIDATE bigendian"

case $TESTGROUP in
	RPC-DRSUAPI)	drsuapi_tests ;;
	RPC-SPOOLSS)	spoolss_tests ;;
	ncacn_ip_tcp)	ncacn_ip_tcp_tests $bindoptions ;;
	ncacn_np)	ncacn_np_tests $bindoptions ;;
	*)		echo "$TESTGROUP is not a known set of tests."
			exit 1;
			;;
esac

exit $all_errs
