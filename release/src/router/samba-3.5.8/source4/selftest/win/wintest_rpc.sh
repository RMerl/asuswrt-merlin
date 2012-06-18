#!/bin/sh

. selftest/test_functions.sh

. selftest/win/wintest_functions.sh

# This variable is defined in the per-hosts .fns file.
. $WINTESTCONF

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_rpc.sh SERVER USERNAME PASSWORD DOMAIN
EOF
exit 1;
fi

server="$1"
username="$2"
password="$3"
domain="$4"
shift 4

ncacn_np_tests="RPC-SRVSVC RPC-UNIXINFO RPC-ECHO RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND"
# These tests fail on ncacn_np: RPC-SPOOLSS RPC-SCHANNEL RPC-JOIN RPC-LSA
# RPC-NETLOGON

ncalrpc_tests="RPC-UNIXINFO RPC-ECHO"
# These tests fail on ncalrpc: RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-DSSETUP
# RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON

ncacn_ip_tcp_tests="RPC-UNIXINFO RPC-ECHO"
# These tests fail on ncacn_ip_tcp: RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-DSSETUP
# RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON

bind_options="seal,padcheck bigendian"

test_type="ncalrpc ncacn_np ncacn_ip_tcp"

all_errs=0

on_error() {
	errstr=$1
	all_errs=`expr $all_errs + 1`

	restore_snapshot "$errstr" "$VM_CFG_PATH"
}

for o in $bind_options; do
	for transport in $test_type; do
		case $transport in
			ncalrpc) rpc_test=$ncalrpc_tests ;;
			ncacn_np) rpc_test=$ncacn_np_tests ;;
			ncacn_ip_tcp) rpc_test=$ncacn_ip_tcp_tests ;;
		esac

		for t in $rpc_test; do
			test_name="$t on $transport with $o"

			$SMBTORTURE_BIN_PATH -U $username%$password \
				-W $domain $transport:$server[$o] \
				$t || on_error "\n$test_name failed."
		done
	done
done

exit $all_errs
