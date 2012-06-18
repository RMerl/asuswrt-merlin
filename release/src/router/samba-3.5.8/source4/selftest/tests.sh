#!/bin/sh
# This script generates a list of testsuites that should be run as part of 
# the Samba 4 test suite.

# The output of this script is parsed by selftest.pl, which then decides 
# which of the tests to actually run. It will, for example, skip all tests 
# listed in selftest/skip or only run a subset during "make quicktest".

# The idea is that this script outputs all of the tests of Samba 4, not 
# just those that are known to pass, and list those that should be skipped 
# or are known to fail in selftest/skip or selftest/knownfail. This makes it 
# very easy to see what functionality is still missing in Samba 4 and makes 
# it possible to run the testsuite against other servers, such as Samba 3 or 
# Windows that have a different set of features.

# The syntax for a testsuite is "-- TEST --" on a single line, followed 
# by the name of the test, the environment it needs and the command to run, all 
# three separated by newlines. All other lines in the output are considered 
# comments.

if [ ! -n "$PERL" ]
then
	PERL=perl
fi

if [ ! -n "$PYTHON" ]
then
	PYTHON=python
fi

plantest() {
	name=$1
	env=$2
	shift 2
	cmdline="$*"
	echo "-- TEST --"
	if [ "$env" = "none" ]; then
		echo "samba4.$name"
	else
		echo "samba4.$name ($env)"
	fi
	echo $env
	echo $cmdline
}

skiptestsuite() {
	name=$1
	reason=$2
	shift 2
	# FIXME: Report this using subunit, but re-adjust the testsuite count somehow
	echo "skipping $name ($reason)"
}

normalize_testname() {
	name=$1
	shift 1
	echo $name | tr "A-Z-" "a-z."
}

planperltest() {
	name=$1
	env=$2
	shift 2
	cmdline="$*"
	if $PERL -e 'eval require Test::More;' > /dev/null 2>&1; then
		plantest "$name" "$env" $PERL $cmdline "|" $TAP2SUBUNIT 
	else
		skiptestsuite "$name" "Test::More not available"
	fi
}

plansmbtorturetest() {
	name=$1
	env=$2
	shift 2
	other_args="$*"
	modname=`normalize_testname $name`
	cmdline="$VALGRIND $smb4torture $other_args $name"
	plantest "$modname" "$env" $cmdline
}

samba4srcdir="`dirname $0`/.."
samba4bindir="$BUILDDIR/bin"
smb4torture="$samba4bindir/smbtorture${EXEEXT}"
if which tap2subunit 2>/dev/null; then
	TAP2SUBUNIT=tap2subunit
else
	TAP2SUBUNIT="$PERL $samba4srcdir/../lib/subunit/tap2subunit"
fi
$smb4torture -V

bbdir=../testprogs/blackbox

prefix_abs="$SELFTEST_PREFIX/s4client"
CONFIGURATION="--configfile=\$SMB_CONF_PATH"

test -d "$prefix_abs" || mkdir "$prefix_abs"

TORTURE_OPTIONS=""
TORTURE_OPTIONS="$TORTURE_OPTIONS $CONFIGURATION"
TORTURE_OPTIONS="$TORTURE_OPTIONS --maximum-runtime=$SELFTEST_MAXTIME"
TORTURE_OPTIONS="$TORTURE_OPTIONS --target=$SELFTEST_TARGET"
TORTURE_OPTIONS="$TORTURE_OPTIONS --basedir=$prefix_abs"
if [ -n "$SELFTEST_VERBOSE" ]; then
	TORTURE_OPTIONS="$TORTURE_OPTIONS --option=torture:progress=no"
fi
TORTURE_OPTIONS="$TORTURE_OPTIONS --format=subunit"
if [ -n "$SELFTEST_QUICK" ]; then
	TORTURE_OPTIONS="$TORTURE_OPTIONS --option=torture:quick=yes"
fi
smb4torture="$smb4torture $TORTURE_OPTIONS"

echo "OPTIONS $TORTURE_OPTIONS"

# Simple tests for LDAP and CLDAP

for options in "" "--option=socket:testnonblock=true" "-U\$USERNAME%\$PASSWORD --option=socket:testnonblock=true" "-U\$USERNAME%\$PASSWORD"; do
    plantest "ldb.ldap with options $options" dc $bbdir/test_ldb.sh ldap \$SERVER_IP $options
done
# see if we support ldaps
if grep ENABLE_GNUTLS.1 include/config.h > /dev/null; then
    for options in "" "-U\$USERNAME%\$PASSWORD"; do
	plantest "ldb.ldaps with options $options" dc $bbdir/test_ldb.sh ldaps \$SERVER_IP $options
    done
fi
plantest "ldb.ldapi with options $options" dc $bbdir/test_ldb.sh ldapi \$PREFIX_ABS/dc/private/ldapi $options
for t in `$smb4torture --list | grep "^LDAP-"`
do
	plansmbtorturetest "$t" dc "-U\$USERNAME%\$PASSWORD" //\$SERVER_IP/_none_
done

# only do the ldb tests when not in quick mode - they are quite slow, and ldb
# is now pretty well tested by the rest of the quick tests anyway
LDBDIR=$samba4srcdir/lib/ldb
export LDBDIR
# Don't run LDB tests when using system ldb, as we won't have ldbtest installed
if [ -f $samba4bindir/ldbtest ]; then
	plantest "ldb" none TEST_DATA_PREFIX=\$PREFIX $LDBDIR/tests/test-tdb.sh
else
	skiptestsuite "ldb" "Using system LDB, ldbtest not available"
fi

# Tests for RPC

# add tests to this list as they start passing, so we test
# that they stay passing
ncacn_np_tests="RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-HANDLES RPC-SAMSYNC RPC-SAMBA3SESSIONKEY RPC-SAMBA3-GETUSERNAME RPC-SAMBA3-LSA RPC-BINDSAMBA3 RPC-NETLOGSAMBA3 RPC-ASYNCBIND RPC-LSALOOKUP RPC-LSA-GETUSER RPC-SCHANNEL2 RPC-AUTHCONTEXT"
ncalrpc_tests="RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-DRSUAPI RPC-ASYNCBIND RPC-LSALOOKUP RPC-LSA-GETUSER RPC-SCHANNEL2 RPC-AUTHCONTEXT"
ncacn_ip_tcp_tests="RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-HANDLES RPC-DSSYNC RPC-ASYNCBIND RPC-LSALOOKUP RPC-LSA-GETUSER RPC-SCHANNEL2 RPC-AUTHCONTEXT RPC-OBJECTUUID"
slow_ncacn_np_tests="RPC-SAMLOGON RPC-SAMR RPC-SAMR-USERS RPC-SAMR-LARGE-DC RPC-SAMR-USERS-PRIVILEGES RPC-SAMR-PASSWORDS RPC-SAMR-PASSWORDS-PWDLASTSET"
slow_ncalrpc_tests="RPC-SAMR RPC-SAMR-PASSWORDS"
slow_ncacn_ip_tcp_tests="RPC-SAMR RPC-SAMR-PASSWORDS RPC-CRACKNAMES"

all_tests="$ncalrpc_tests $ncacn_np_tests $ncacn_ip_tcp_tests $slow_ncalrpc_tests $slow_ncacn_np_tests $slow_ncacn_ip_tcp_tests RPC-LSA-SECRETS RPC-SAMBA3-SHARESEC RPC-COUNTCALLS"

# Make sure all tests get run
for t in `$smb4torture --list | grep "^RPC-"`
do
	echo $all_tests | grep "$t"  > /dev/null
	if [ $? -ne 0 ]
	then
		auto_rpc_tests="$auto_rpc_tests $t"
	fi
done

for bindoptions in seal,padcheck $VALIDATE bigendian; do
 for transport in ncalrpc ncacn_np ncacn_ip_tcp; do
     env="dc"
     case $transport in
	 ncalrpc) tests=$ncalrpc_tests;env="dc:local" ;;
	 ncacn_np) tests=$ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$ncacn_ip_tcp_tests ;;
     esac
   for t in $tests; do
    plantest "`normalize_testname $t` on $transport with $bindoptions" $env $VALGRIND $smb4torture $transport:"\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"
   done
   plantest "rpc.samba3.sharesec on $transport with $bindoptions" $env $VALGRIND $smb4torture $transport:"\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN --option=torture:share=tmp RPC-SAMBA3-SHARESEC "$*"
 done
done

for bindoptions in "" $VALIDATE bigendian; do
 for t in $auto_rpc_tests; do
  plantest "`normalize_testname $t` with $bindoptions" dc $VALGRIND $smb4torture "\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"
 done
done

t="RPC-COUNTCALLS"
plantest "`normalize_testname $t`" dc:local $VALGRIND $smb4torture "\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"

for bindoptions in connect $VALIDATE ; do
 for transport in ncalrpc ncacn_np ncacn_ip_tcp; do
     env="dc"
     case $transport in
	 ncalrpc) tests=$slow_ncalrpc_tests; env="dc:local" ;;
	 ncacn_np) tests=$slow_ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$slow_ncacn_ip_tcp_tests ;;
     esac
   for t in $tests; do
    plantest "`normalize_testname $t` on $transport with $bindoptions" $env $VALGRIND $smb4torture $transport:"\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"
   done
 done
done


# Tests for the NET API

net=`$smb4torture --list | grep "^NET-"`

for t in $net; do
    plansmbtorturetest "$t" dc "\$SERVER[$VALIDATE]" -U"\$USERNAME"%"\$PASSWORD" -W "\$DOMAIN" "$*"
done

# Tests for session keys
# FIXME: Integrate these into a single smbtorture test

bindoptions=""
transport="ncacn_np"
for ntlmoptions in \
        "-k no --option=usespnego=yes" \
        "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no" \
        "-k no --option=usespnego=yes --option=ntlmssp_client:56bit=yes" \
        "-k no --option=usespnego=yes --option=ntlmssp_client:56bit=no" \
        "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=yes" \
        "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=no" \
        "-k no --option=usespnego=yes --option=clientntlmv2auth=yes" \
        "-k no --option=usespnego=yes --option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no" \
        "-k no --option=usespnego=yes --option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=yes" \
        "-k no --option=usespnego=no --option=clientntlmv2auth=yes" \
        "-k no --option=gensec:spnego=no --option=clientntlmv2auth=yes" \
        "-k no --option=usespnego=no"; do
	name="rpc.lsa.secrets on $transport with $bindoptions with $ntlmoptions"
   plantest "$name" dc $smb4torture $transport:"\$SERVER[$bindoptions]"  $ntlmoptions -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN --option=gensec:target_hostname=\$NETBIOSNAME RPC-LSA-SECRETS "$*"
done
plantest "rpc.lsa.secrets on $transport with $bindoptions with Kerberos" dc $smb4torture $transport:"\$SERVER[$bindoptions]" -k yes -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN "--option=gensec:target_hostname=\$NETBIOSNAME" RPC-LSA-SECRETS "$*"
plantest "rpc.lsa.secrets on $transport with $bindoptions with Kerberos - use target principal" dc $smb4torture $transport:"\$SERVER[$bindoptions]" -k yes -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN "--option=clientusespnegoprincipal=yes" "--option=gensec:target_hostname=\$NETBIOSNAME" RPC-LSA-SECRETS "$*"
plantest "rpc.lsa.secrets on $transport with Kerberos - use Samba3 style login" dc $smb4torture $transport:"\$SERVER" -k yes -U"\$USERNAME"%"\$PASSWORD" -W "\$DOMAIN" "--option=gensec:fake_gssapi_krb5=yes" "--option=gensec:gssapi_krb5=no" "--option=gensec:target_hostname=\$NETBIOSNAME" "RPC-LSA-SECRETS-none*" "$*"
plantest "rpc.lsa.secrets on $transport with Kerberos - use Samba3 style login, use target principal" dc $smb4torture $transport:"\$SERVER" -k yes -U"\$USERNAME"%"\$PASSWORD" -W "\$DOMAIN" "--option=clientusespnegoprincipal=yes" "--option=gensec:fake_gssapi_krb5=yes" "--option=gensec:gssapi_krb5=no" "--option=gensec:target_hostname=\$NETBIOSNAME" "RPC-LSA-SECRETS-none*" "$*"

# Echo tests
transports="ncacn_np ncacn_ip_tcp ncalrpc"

for transport in $transports; do
 for bindoptions in connect spnego spnego,sign spnego,seal $VALIDATE padcheck bigendian bigendian,seal; do
  for ntlmoptions in \
        "--option=socket:testnonblock=True --option=torture:quick=yes"; do
   env="dc"
   if test x"$transport" = x"ncalrpc"; then
      env="dc:local"
   fi
   plantest "rpc.echo on $transport with $bindoptions and $ntlmoptions" $env $smb4torture $transport:"\$SERVER[$bindoptions]" $ntlmoptions -U"\$USERNAME"%"\$PASSWORD" -W "\$DOMAIN" RPC-ECHO "$*"
  done
 done
done

for transport in $transports; do
 for bindoptions in sign seal; do
  for ntlmoptions in \
        "--option=ntlmssp_client:ntlm2=yes --option=torture:quick=yes" \
        "--option=ntlmssp_client:ntlm2=no  --option=torture:quick=yes" \
        "--option=ntlmssp_client:ntlm2=yes --option=ntlmssp_client:128bit=no --option=torture:quick=yes" \
        "--option=ntlmssp_client:ntlm2=no  --option=ntlmssp_client:128bit=no --option=torture:quick=yes" \
        "--option=ntlmssp_client:ntlm2=yes --option=ntlmssp_client:keyexchange=no --option=torture:quick=yes" \
        "--option=ntlmssp_client:ntlm2=no  --option=ntlmssp_client:keyexchange=no  --option=torture:quick=yes" \
        "--option=clientntlmv2auth=yes  --option=ntlmssp_client:keyexchange=no  --option=torture:quick=yes" \
        "--option=clientntlmv2auth=yes  --option=ntlmssp_client:128bit=no --option=ntlmssp_client:keyexchange=yes  --option=torture:quick=yes" \
        "--option=clientntlmv2auth=yes  --option=ntlmssp_client:128bit=no --option=ntlmssp_client:keyexchange=no  --option=torture:quick=yes" \
    ; do
   env="dc"
   if test x"$transport" = x"ncalrpc"; then
      env="dc:local"
   fi
   plantest "rpc.echo on $transport with $bindoptions and $ntlmoptions" $env $smb4torture $transport:"\$SERVER[$bindoptions]" $ntlmoptions -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN RPC-ECHO "$*"
  done
 done
done

plantest "rpc.echo on ncacn_np over smb2" dc $smb4torture ncacn_np:"\$SERVER[smb2]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN RPC-ECHO "$*"

plantest "ntp.signd" dc $smb4torture ncacn_np:"\$SERVER" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN NTP-SIGND "$*" --configfile=st/dc/etc/smb.conf

# Tests against the NTVFS POSIX backend
NTVFSARGS=""
NTVFSARGS="${NTVFSARGS} --option=torture:sharedelay=100000"
NTVFSARGS="${NTVFSARGS} --option=torture:oplocktimeout=3"
NTVFSARGS="${NTVFSARGS} --option=torture:writetimeupdatedelay=500000"

smb2=`$smb4torture --list | grep "^SMB2-" | xargs`
#The QFILEINFO-IPC test needs to be on ipc$
raw=`$smb4torture --list | grep "^RAW-" | grep -v "RAW-QFILEINFO-IPC"| xargs`
base=`$smb4torture --list | grep "^BASE-" | xargs`

for t in $base $raw $smb2; do
    plansmbtorturetest "$t" dc $ADDARGS //\$SERVER/tmp -U"\$USERNAME"%"\$PASSWORD" $NTVFSARGS
done

plansmbtorturetest "RAW-QFILEINFO-IPC" dc $ADDARGS //\$SERVER/ipc$ -U"\$USERNAME"%"\$PASSWORD"

rap=`$smb4torture --list | grep "^RAP-" | xargs`
for t in $rap; do
    plansmbtorturetest "$t" dc $ADDARGS //\$SERVER/IPC\\\$ -U"\$USERNAME"%"\$PASSWORD"
done

# Tests against the NTVFS CIFS backend
for t in $base $raw; do
    plantest "ntvfs.cifs.`normalize_testname $t`" dc $VALGRIND $smb4torture //\$NETBIOSNAME/cifs -U"\$USERNAME"%"\$PASSWORD" $NTVFSARGS $t
done

# Local tests

for t in `$smb4torture --list | grep "^LOCAL-" | xargs`; do
	plansmbtorturetest "$t" none ncalrpc: "$*"
done

tdbtorture4="$samba4bindir/tdbtorture${EXEEXT}"
if test -f $tdbtorture4
then
	plantest "tdb.stress" none $VALGRIND $tdbtorture4
else
	skiptestsuite "tdb.stress" "Using system TDB, tdbtorture not available"
fi

# Pidl tests

for f in $samba4srcdir/../pidl/tests/*.pl; do
	 planperltest "pidl.`basename $f .pl`" none $f
done
planperltest "selftest.samba4.pl" none $samba4srcdir/../selftest/test_samba4.pl

# Blackbox Tests:
# tests that interact directly with the command-line tools rather than using 
# the API. These mainly test that the various command-line options of commands 
# work correctly.

plantest "blackbox.ndrdump" none $samba4srcdir/librpc/tests/test_ndrdump.sh
plantest "blackbox.net" dc $samba4srcdir/utils/tests/test_net.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$DOMAIN"
plantest "blackbox.kinit" dc $bbdir/test_kinit.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$REALM" "\$DOMAIN" "$PREFIX" $CONFIGURATION 
plantest "blackbox.passwords" dc $bbdir/test_passwords.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$REALM" "\$DOMAIN" "$PREFIX" --configfile=st/dc/etc/smb.conf
plantest "blackbox.export.keytab" dc $bbdir/test_export_keytab.sh "\$SERVER" "\$USERNAME" "\$REALM" "\$DOMAIN" "$PREFIX" --configfile=st/dc/etc/smb.conf
plantest "blackbox.cifsdd" dc $samba4srcdir/client/tests/test_cifsdd.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$DOMAIN" 
plantest "blackbox.nmblookup" dc $samba4srcdir/utils/tests/test_nmblookup.sh "\$NETBIOSNAME" "\$NETBIOSALIAS" "\$SERVER" "\$SERVER_IP" 
plantest "blackbox.nmblookup" member $samba4srcdir/utils/tests/test_nmblookup.sh "\$NETBIOSNAME" "\$NETBIOSALIAS" "\$SERVER" "\$SERVER_IP"
plantest "blackbox.locktest" dc $samba4srcdir/torture/tests/test_locktest.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$DOMAIN" "$PREFIX"
plantest "blackbox.masktest" dc $samba4srcdir/torture/tests/test_masktest.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$DOMAIN" "$PREFIX"
plantest "blackbox.gentest" dc $samba4srcdir/torture/tests/test_gentest.sh "\$SERVER" "\$USERNAME" "\$PASSWORD" "\$DOMAIN" "$PREFIX"
plantest "blackbox.wbinfo" dc:local $samba4srcdir/../nsswitch/tests/test_wbinfo.sh "\$DOMAIN" "\$USERNAME" "\$PASSWORD" "dc"
plantest "blackbox.wbinfo" member:local $samba4srcdir/../nsswitch/tests/test_wbinfo.sh "\$DOMAIN" "\$DC_USERNAME" "\$DC_PASSWORD" "member"

# Tests using the "Simple" NTVFS backend

for t in "BASE-RW1"; do
    plantest "ntvfs.simple.`normalize_testname $t`" dc $VALGRIND $smb4torture $ADDARGS //\$SERVER/simple -U"\$USERNAME"%"\$PASSWORD" $t
done

# Domain Member Tests

plantest "rpc.echo against member server with local creds" member $VALGRIND $smb4torture ncacn_np:"\$NETBIOSNAME" -U"\$NETBIOSNAME/\$USERNAME"%"\$PASSWORD" RPC-ECHO "$*"
plantest "rpc.echo against member server with domain creds" member $VALGRIND $smb4torture ncacn_np:"\$NETBIOSNAME" -U"\$DOMAIN/\$DC_USERNAME"%"\$DC_PASSWORD" RPC-ECHO "$*"
plantest "rpc.samr against member server with local creds" member $VALGRIND $smb4torture ncacn_np:"\$NETBIOSNAME" -U"\$NETBIOSNAME/\$USERNAME"%"\$PASSWORD" "RPC-SAMR" "$*"
plantest "rpc.samr.users against member server with local creds" member $VALGRIND $smb4torture ncacn_np:"\$NETBIOSNAME" -U"\$NETBIOSNAME/\$USERNAME"%"\$PASSWORD" "RPC-SAMR-USERS" "$*"
plantest "rpc.samr.passwords against member server with local creds" member $VALGRIND $smb4torture ncacn_np:"\$NETBIOSNAME" -U"\$NETBIOSNAME/\$USERNAME"%"\$PASSWORD" "RPC-SAMR-PASSWORDS" "$*"
plantest "blackbox.smbclient against member server with local creds" member $samba4srcdir/client/tests/test_smbclient.sh "\$NETBIOSNAME" "\$USERNAME" "\$PASSWORD" "\$NETBIOSNAME" "$PREFIX" 

# Tests SMB signing

for mech in \
	"-k no" \
	"-k no --option=usespnego=no" \
	"-k no --option=gensec:spengo=no" \
	"-k yes" \
	"-k yes --option=gensec:fake_gssapi_krb5=yes --option=gensec:gssapi_krb5=no"; do
   for signing in \
	"--signing=on" \
	"--signing=required"; do

	signoptions="$mech $signing"
	name="smb.signing on with $signoptions"
	plantest "$name" dc $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp $signoptions -U"\$USERNAME"%"\$PASSWORD" BASE-XCOPY "$*"
   done
done

for mech in \
	"-k no" \
	"-k no --option=usespnego=no" \
	"-k no --option=gensec:spengo=no" \
	"-k yes" \
	"-k yes --option=gensec:fake_gssapi_krb5=yes --option=gensec:gssapi_krb5=no"; do
	signoptions="$mech --signing=off"
	name="smb.signing on with $signoptions"
	plantest "$name domain-creds" member $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp $signoptions -U"\$DC_USERNAME"%"\$DC_PASSWORD" BASE-XCOPY "$*"
done
for mech in \
	"-k no" \
	"-k no --option=usespnego=no" \
	"-k no --option=gensec:spengo=no"; do
	signoptions="$mech --signing=off"
	name="smb.signing on with $signoptions"
	plantest "$name local-creds" member $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp $signoptions -U"\$NETBIOSNAME/\$USERNAME"%"\$PASSWORD" BASE-XCOPY "$*"
done
plantest "smb.signing --signing=yes anon" dc $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp -k no --signing=yes -U% BASE-XCOPY "$*"
plantest "smb.signing --signing=required anon" dc $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp -k no --signing=required -U% BASE-XCOPY "$*"
plantest "smb.signing --signing=no anon" member $VALGRIND $smb4torture //"\$NETBIOSNAME"/tmp -k no --signing=no -U% BASE-XCOPY "$*"

NBT_TESTS=`$smb4torture --list | grep "^NBT-" | xargs`

for t in $NBT_TESTS; do
	plansmbtorturetest "$t" dc //\$SERVER/_none_ -U\$USERNAME%\$PASSWORD 
done

WB_OPTS="--option=\"torture:strict mode=no\""
WB_OPTS="${WB_OPTS} --option=\"torture:timelimit=1\""
WB_OPTS="${WB_OPTS} --option=\"torture:winbindd separator=/\""
WB_OPTS="${WB_OPTS} --option=\"torture:winbindd netbios name=\$SERVER\""
WB_OPTS="${WB_OPTS} --option=\"torture:winbindd netbios domain=\$DOMAIN\""

WINBIND_STRUCT_TESTS=`$smb4torture --list | grep "^WINBIND-STRUCT" | xargs`
WINBIND_NDR_TESTS=`$smb4torture --list | grep "^WINBIND-NDR" | xargs`
for env in dc member; do
	for t in $WINBIND_STRUCT_TESTS; do
		plansmbtorturetest $t $env $WB_OPTS //_none_/_none_
	done

	for t in $WINBIND_NDR_TESTS; do
		plansmbtorturetest $t $env $WB_OPTS //_none_/_none_
	done
done

nsstest4="$samba4bindir/nsstest${EXEEXT}"
if test -f $nsstest4
then
	plantest "nss.test using winbind" member $VALGRIND $nsstest4 $samba4bindir/shared/libnss_winbind.so
fi

SUBUNITRUN="$VALGRIND $PYTHON $samba4srcdir/scripting/bin/subunitrun"
plantest "ldb.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/lib/ldb/tests/python/" $SUBUNITRUN api
plantest "credentials.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/auth/credentials/tests" $SUBUNITRUN bindings
plantest "gensec.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/auth/gensec/tests" $SUBUNITRUN bindings
plantest "registry.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/lib/registry/tests/" $SUBUNITRUN bindings
plantest "tdb.python" none PYTHONPATH="$PYTHONPATH:../lib/tdb/python/tests" $SUBUNITRUN simple
plantest "auth.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/auth/tests/" $SUBUNITRUN bindings
plantest "security.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/libcli/security/tests" $SUBUNITRUN bindings
plantest "misc.python" none $SUBUNITRUN samba.tests.dcerpc.misc
plantest "param.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/param/tests" $SUBUNITRUN bindings
plantest "upgrade.python" none $SUBUNITRUN samba.tests.upgrade
plantest "samba.python" none $SUBUNITRUN samba.tests
plantest "provision.python" none $SUBUNITRUN samba.tests.provision
plantest "samba3.python" none $SUBUNITRUN samba.tests.samba3
plantest "samr.python" dc:local $SUBUNITRUN samba.tests.dcerpc.sam
plantest "dcerpc.bare.python" dc:local $SUBUNITRUN samba.tests.dcerpc.bare
plantest "unixinfo.python" dc:local $SUBUNITRUN samba.tests.dcerpc.unix
plantest "samdb.python" none $SUBUNITRUN samba.tests.samdb
plantest "shares.python" none $SUBUNITRUN samba.tests.shares
plantest "messaging.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/lib/messaging/tests" $SUBUNITRUN bindings
plantest "samba3sam.python" none PYTHONPATH="$PYTHONPATH:$samba4srcdir/dsdb/samdb/ldb_modules/tests" $SUBUNITRUN samba3sam
plantest "subunit.python" none $SUBUNITRUN subunit
plantest "rpcecho.python" dc:local $SUBUNITRUN samba.tests.dcerpc.rpcecho
plantest "winreg.python" dc:local $SUBUNITRUN -U\$USERNAME%\$PASSWORD samba.tests.dcerpc.registry
plantest "ldap.python" dc PYTHONPATH="$PYTHONPATH:../lib/subunit/python" $PYTHON $samba4srcdir/lib/ldb/tests/python/ldap.py $CONFIGURATION \$SERVER -U\$USERNAME%\$PASSWORD -W \$DOMAIN
plantest "ldap.possibleInferiors.python" dc $PYTHON $samba4srcdir/dsdb/samdb/ldb_modules/tests/possibleinferiors.py $CONFIGURATION ldap://\$SERVER -U\$USERNAME%\$PASSWORD -W \$DOMAIN
plantest "ldap.secdesc.python" dc PYTHONPATH="$PYTHONPATH:../lib/subunit/python" $PYTHON $samba4srcdir/lib/ldb/tests/python/sec_descriptor.py $CONFIGURATION \$SERVER -U\$USERNAME%\$PASSWORD -W \$DOMAIN
plantest "blackbox.samba3dump" none $PYTHON $samba4srcdir/scripting/bin/samba3dump $samba4srcdir/../testdata/samba3
rm -rf $PREFIX/upgrade
plantest "blackbox.upgrade" none $PYTHON $samba4srcdir/setup/upgrade $CONFIGURATION --targetdir=$PREFIX/upgrade $samba4srcdir/../testdata/samba3 ../testdata/samba3/smb.conf
rm -rf $PREFIX/provision
mkdir $PREFIX/provision
plantest "blackbox.provision.py" none PYTHON="$PYTHON" $samba4srcdir/setup/tests/blackbox_provision.sh "$PREFIX/provision"
plantest "blackbox.provision-backend.py" none PYTHON="$PYTHON" $samba4srcdir/setup/tests/blackbox_provision-backend.sh "$PREFIX/provision"
plantest "blackbox.setpassword.py" none PYTHON="$PYTHON" $samba4srcdir/setup/tests/blackbox_setpassword.sh "$PREFIX/provision"
plantest "blackbox.newuser.py" none PYTHON="$PYTHON" $samba4srcdir/setup/tests/blackbox_newuser.sh "$PREFIX/provision" 
