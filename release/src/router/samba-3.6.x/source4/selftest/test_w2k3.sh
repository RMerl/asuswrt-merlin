#!/bin/sh

# tests that should pass against a w2k3 DC, as administrator

# add tests to this list as they start passing, so we test
# that they stay passing
ncacn_np_tests="RPC-SCHANNEL RPC-DSSETUP RPC-EPMAPPER RPC-SAMR RPC-WKSSVC RPC-SRVSVC RPC-EVENTLOG RPC-NETLOGON RPC-LSA RPC-SAMLOGON RPC-SAMSYNC RPC-MULTIBIND RPC-WINREG RPC-SPOOLSS RPC-SPOOLSS-WIN"
ncacn_ip_tcp_tests="RPC-SCHANNEL RPC-EPMAPPER RPC-SAMR RPC-NETLOGON RPC-LSA RPC-SAMLOGON RPC-SAMSYNC RPC-MULTIBIND"

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_w2k3.sh SERVER USERNAME PASSWORD DOMAIN REALM
EOF
exit 1;
fi

server="$1"
username="$2"
password="$3"
domain="$4"
realm="$5"
shift 5

incdir=`dirname $0`
. $incdir/test_functions.sh

OPTIONS="-U$username%$password -W $domain --option realm=$realm"

name="RPC-SPOOLSS on ncacn_np"
testit "$name" rpc bin/smbtorture $TORTURE_OPTIONS ncacn_np:"$server" $OPTIONS RPC-SPOOLSS "$*"

for bindoptions in padcheck connect sign seal ntlm,sign ntlm,seal $VALIDATE bigendian; do
   for transport in ncacn_ip_tcp ncacn_np; do
     case $transport in
	 ncacn_np) tests=$ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$ncacn_ip_tcp_tests ;;
     esac
   for t in $tests; do
    name="$t on $transport with $bindoptions"
    testit "$name" rpc bin/smbtorture $TORTURE_OPTIONS $transport:"$server[$bindoptions]" $OPTIONS $t "$*"
   done
 done
done

name="RPC-DRSUAPI on ncacn_ip_tcp with seal"
testit "$name" rpc bin/smbtorture $TORTURE_OPTIONS ncacn_ip_tcp:"$server[seal]" $OPTIONS RPC-DRSUAPI "$*"
name="RPC-DRSUAPI on ncacn_ip_tcp with seal,bigendian"
testit "$name" rpc bin/smbtorture $TORTURE_OPTIONS ncacn_ip_tcp:"$server[seal,bigendian]" $OPTIONS RPC-DRSUAPI "$*"
