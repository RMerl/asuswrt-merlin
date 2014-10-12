#!/usr/bin/python
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

import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../selftest"))
from selftesthelpers import *
import subprocess

samba4srcdir = source4dir()
samba4bindir = bindir()
smb4torture = binpath("smbtorture")
smb4torture_testsuite_list = subprocess.Popen([smb4torture, "--list-suites"], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate("")[0].splitlines()
validate = os.getenv("VALIDATE", "")
if validate:
    validate_list = [validate]
else:
    validate_list = []

def plansmbtorturetestsuite(name, env, options):
    modname = "samba4.%s" % name
    cmdline = "%s $LISTOPT %s %s" % (valgrindify(smb4torture), options, name)
    plantestsuite_loadlist(modname, env, cmdline)

def smb4torture_testsuites(prefix):
    return filter(lambda x: x.startswith(prefix), smb4torture_testsuite_list)

subprocess.call([smb4torture, "-V"])

bbdir = os.path.join(srcdir(), "testprogs/blackbox")

configuration = "--configfile=$SMB_CONF_PATH"

torture_options = [configuration, "--maximum-runtime=$SELFTEST_MAXTIME", "--target=$SELFTEST_TARGET", "--basedir=$SELFTEST_TMPDIR"]
if not os.getenv("SELFTEST_VERBOSE"):
    torture_options.append("--option=torture:progress=no")
torture_options.append("--format=subunit")
if os.getenv("SELFTEST_QUICK"):
    torture_options.append("--option=torture:quick=yes")
smb4torture += " " + " ".join(torture_options)

print "OPTIONS %s" % " ".join(torture_options)

# Simple tests for LDAP and CLDAP
for options in ['-U"$USERNAME%$PASSWORD" --option=socket:testnonblock=true', '-U"$USERNAME%$PASSWORD"', '-U"$USERNAME%$PASSWORD" -k yes', '-U"$USERNAME%$PASSWORD" -k no', '-U"$USERNAME%$PASSWORD" -k no --sign', '-U"$USERNAME%$PASSWORD" -k no --encrypt', '-U"$USERNAME%$PASSWORD" -k yes --encrypt', '-U"$USERNAME%$PASSWORD" -k yes --sign']:
    plantestsuite("samba4.ldb.ldap with options %s(dc)" % options, "dc", "%s/test_ldb.sh ldap $SERVER %s" % (bbdir, options))

# see if we support ldaps
try:
    config_h = os.environ["CONFIG_H"]
except KeyError:
    config_h = os.path.join(samba4bindir, "default/source4/include/config.h")
f = open(config_h, 'r')
try:
    have_tls_support = ("ENABLE_GNUTLS 1" in f.read())
finally:
    f.close()

if have_tls_support:
    for options in ['-U"$USERNAME%$PASSWORD"']:
        plantestsuite("samba4.ldb.ldaps with options %s(dc)" % options, "dc",
                "%s/test_ldb.sh ldaps $SERVER_IP %s" % (bbdir, options))

for options in ['-U"$USERNAME%$PASSWORD"']:
    plantestsuite("samba4.ldb.ldapi with options %s(dc:local)" % options, "dc:local",
            "%s/test_ldb.sh ldapi $PREFIX_ABS/dc/private/ldapi %s" % (bbdir, options))

for t in smb4torture_testsuites("ldap."):
    plansmbtorturetestsuite(t, "dc", '-U"$USERNAME%$PASSWORD" //$SERVER_IP/_none_')

ldbdir = os.path.join(samba4srcdir, "lib/ldb")
# Don't run LDB tests when using system ldb, as we won't have ldbtest installed
if os.path.exists(os.path.join(samba4bindir, "ldbtest")):
    plantestsuite("ldb.base", "none", "%s/tests/test-tdb.sh" % ldbdir,
                  allow_empty_output=True)
else:
    skiptestsuite("ldb.base", "Using system LDB, ldbtest not available")

# Tests for RPC

# add tests to this list as they start passing, so we test
# that they stay passing
ncacn_np_tests = ["rpc.schannel", "rpc.join", "rpc.lsa", "rpc.dssetup", "rpc.altercontext", "rpc.multibind", "rpc.netlogon", "rpc.handles", "rpc.samsync", "rpc.samba3-sessionkey", "rpc.samba3-getusername", "rpc.samba3-lsa", "rpc.samba3-bind", "rpc.samba3-netlogon", "rpc.asyncbind", "rpc.lsalookup", "rpc.lsa-getuser", "rpc.schannel2", "rpc.authcontext"]
ncalrpc_tests = ["rpc.schannel", "rpc.join", "rpc.lsa", "rpc.dssetup", "rpc.altercontext", "rpc.multibind", "rpc.netlogon", "rpc.drsuapi", "rpc.asyncbind", "rpc.lsalookup", "rpc.lsa-getuser", "rpc.schannel2", "rpc.authcontext"]
drs_rpc_tests = smb4torture_testsuites("drs.rpc")
ncacn_ip_tcp_tests = ["rpc.schannel", "rpc.join", "rpc.lsa", "rpc.dssetup", "rpc.altercontext", "rpc.multibind", "rpc.netlogon", "rpc.handles", "rpc.asyncbind", "rpc.lsalookup", "rpc.lsa-getuser", "rpc.schannel2", "rpc.authcontext", "rpc.objectuuid"] + drs_rpc_tests
slow_ncacn_np_tests = ["rpc.samlogon", "rpc.samr.users", "rpc.samr.large-dc", "rpc.samr.users.privileges", "rpc.samr.passwords", "rpc.samr.passwords.pwdlastset"]
slow_ncacn_ip_tcp_tests = ["rpc.samr", "rpc.cracknames"]

all_rpc_tests = ncalrpc_tests + ncacn_np_tests + ncacn_ip_tcp_tests + slow_ncacn_np_tests + slow_ncacn_ip_tcp_tests + ["rpc.lsa.secrets", "rpc.pac", "rpc.samba3-sharesec", "rpc.countcalls"]

# Make sure all tests get run
rpc_tests = smb4torture_testsuites("rpc.")
auto_rpc_tests = filter(lambda t: t not in all_rpc_tests, rpc_tests)

for bindoptions in ["seal,padcheck"] + validate_list + ["bigendian"]:
    for transport in ["ncalrpc", "ncacn_np", "ncacn_ip_tcp"]:
        env = "dc"
        if transport == "ncalrpc":
            tests = ncalrpc_tests
            env = "dc:local"
        elif transport == "ncacn_np":
            tests = ncacn_np_tests
        elif transport == "ncacn_ip_tcp":
            tests = ncacn_ip_tcp_tests
        for t in tests:
            plantestsuite_loadlist("samba4.%s on %s with %s" % (t, transport, bindoptions), env, [valgrindify(smb4torture), "$LISTOPT", "%s:$SERVER[%s]" % (transport, bindoptions), '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])
        plantestsuite_loadlist("samba4.rpc.samba3.sharesec on %s with %s" % (transport, bindoptions), env, [valgrindify(smb4torture), "$LISTOPT", "%s:$SERVER[%s]" % (transport, bindoptions), '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', '--option=torture:share=tmp', 'rpc.samba3-sharesec'])

for bindoptions in [""] + validate_list + ["bigendian"]:
    for t in auto_rpc_tests:
        plantestsuite_loadlist("samba4.%s with %s" % (t, bindoptions), "dc", [valgrindify(smb4torture), "$LISTOPT", "$SERVER[%s]" % bindoptions, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])

t = "rpc.countcalls"
plantestsuite_loadlist("samba4.%s" % t, "dc:local", [valgrindify(smb4torture), "$LISTOPT", "$SERVER[%s]" % bindoptions, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])

for transport in ["ncacn_np", "ncacn_ip_tcp"]:
    env = "dc"
    if transport == "ncacn_np":
        tests = slow_ncacn_np_tests
    elif transport == "ncacn_ip_tcp":
        tests = slow_ncacn_ip_tcp_tests
    for t in tests:
        plantestsuite_loadlist("samba4.%s on %s" % (t, transport), env, [valgrindify(smb4torture), "$LISTOPT", "%s:$SERVER" % transport, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])

# Tests for the DFS referral calls implementation
for t in smb4torture_testsuites("dfs."):
    plansmbtorturetestsuite(t, "dc", '//$SERVER/ipc\$ -U$USERNAME%$PASSWORD')

# Tests for the NET API (net.api.become.dc tested below against all the roles)
net_tests = filter(lambda x: "net.api.become.dc" not in x, smb4torture_testsuites("net."))
for t in net_tests:
    plansmbtorturetestsuite(t, "dc", '$SERVER[%s] -U$USERNAME%%$PASSWORD -W $DOMAIN' % validate)

# Tests for session keys and encryption of RPC pipes
# FIXME: Integrate these into a single smbtorture test

transport = "ncacn_np"
for ntlmoptions in [
    "-k no --option=usespnego=yes",
    "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no",
    "-k no --option=usespnego=yes --option=ntlmssp_client:56bit=yes",
    "-k no --option=usespnego=yes --option=ntlmssp_client:56bit=no",
    "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=yes",
    "-k no --option=usespnego=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=no",
    "-k no --option=usespnego=yes --option=clientntlmv2auth=yes",
    "-k no --option=usespnego=yes --option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no",
    "-k no --option=usespnego=yes --option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:56bit=yes",
    "-k no --option=usespnego=no --option=clientntlmv2auth=yes",
    "-k no --option=gensec:spnego=no --option=clientntlmv2auth=yes",
    "-k no --option=usespnego=no"]:
    name = "rpc.lsa.secrets on %s with with %s" % (transport, ntlmoptions)
    plantestsuite_loadlist("samba4.%s" % name, "dc", [smb4torture, "$LISTOPT", "%s:$SERVER[]" % (transport), ntlmoptions, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', '--option=gensec:target_hostname=$NETBIOSNAME', 'rpc.lsa.secrets'])

transports = ["ncacn_np", "ncacn_ip_tcp"]

#Kerberos varies between functional levels, so it is important to check this on all of them
for env in ["dc", "fl2000dc", "fl2003dc", "fl2008r2dc"]:
    transport = "ncacn_np"
    plantestsuite_loadlist("samba4.rpc.pac on %s" % (transport,), env, [smb4torture, "$LISTOPT", "%s:$SERVER[]" % (transport, ), '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.pac'])
    for transport in transports:
        plantestsuite_loadlist("samba4.rpc.lsa.secrets on %s with Kerberos" % (transport,), env, [smb4torture, "$LISTOPT", "%s:$SERVER[]" % (transport, ), '-k', 'yes', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', '--option=gensec:target_hostname=$NETBIOSNAME', 'rpc.lsa.secrets'])
        plantestsuite_loadlist("samba4.rpc.lsa.secrets on %s with Kerberos - use target principal" % (transport,), env, [smb4torture, "$LISTOPT", "%s:$SERVER[]" % (transport, ), '-k', 'yes', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', "--option=clientusespnegoprincipal=yes", '--option=gensec:target_hostname=$NETBIOSNAME', 'rpc.lsa.secrets'])
        plantestsuite_loadlist("samba4.rpc.lsa.secrets on %s with Kerberos - use Samba3 style login" % transport, env, [smb4torture, "$LISTOPT", "%s:$SERVER" % transport, '-k', 'yes', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', "--option=gensec:fake_gssapi_krb5=yes", '--option=gensec:gssapi_krb5=no', '--option=gensec:target_hostname=$NETBIOSNAME', "rpc.lsa.secrets.none*"])
        plantestsuite_loadlist("samba4.rpc.lsa.secrets on %s with Kerberos - use Samba3 style login, use target principal" % transport, env, [smb4torture, "$LISTOPT", "%s:$SERVER" % transport, '-k', 'yes', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', "--option=clientusespnegoprincipal=yes", '--option=gensec:fake_gssapi_krb5=yes', '--option=gensec:gssapi_krb5=no', '--option=gensec:target_hostname=$NETBIOSNAME', "rpc.lsa.secrets.none*"])
        plantestsuite_loadlist("samba4.rpc.echo on %s" % (transport, ), env, [smb4torture, "$LISTOPT", "%s:$SERVER[]" % (transport,), '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.echo'])

        # Echo tests test bulk Kerberos encryption of DCE/RPC
        for bindoptions in ["connect", "spnego", "spnego,sign", "spnego,seal"] + validate_list + ["padcheck", "bigendian", "bigendian,seal"]:
            echooptions = "--option=socket:testnonblock=True --option=torture:quick=yes -k yes"
            plantestsuite_loadlist("samba4.rpc.echo on %s with %s and %s" % (transport, bindoptions, echooptions), env, [smb4torture, "$LISTOPT", "%s:$SERVER[%s]" % (transport, bindoptions), echooptions, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.echo'])
    plansmbtorturetestsuite("net.api.become.dc", env, '$SERVER[%s] -U$USERNAME%%$PASSWORD -W $DOMAIN' % validate)

for bindoptions in ["sign", "seal"]:
    env = "dc"
    plantestsuite_loadlist("samba4.rpc.backupkey with %s" % (bindoptions), env, [smb4torture, "$LISTOPT", "ncacn_np:$SERVER[%s]" % ( bindoptions), '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.backupkey'])

for transport in transports:
    for bindoptions in ["sign", "seal"]:
        for ntlmoptions in [
        "--option=ntlmssp_client:ntlm2=yes --option=torture:quick=yes",
        "--option=ntlmssp_client:ntlm2=no --option=torture:quick=yes",
        "--option=ntlmssp_client:ntlm2=yes --option=ntlmssp_client:128bit=no --option=torture:quick=yes",
        "--option=ntlmssp_client:ntlm2=no --option=ntlmssp_client:128bit=no --option=torture:quick=yes",
        "--option=ntlmssp_client:ntlm2=yes --option=ntlmssp_client:keyexchange=no --option=torture:quick=yes",
        "--option=ntlmssp_client:ntlm2=no --option=ntlmssp_client:keyexchange=no --option=torture:quick=yes",
        "--option=clientntlmv2auth=yes --option=ntlmssp_client:keyexchange=no --option=torture:quick=yes",
        "--option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:keyexchange=yes --option=torture:quick=yes",
        "--option=clientntlmv2auth=yes --option=ntlmssp_client:128bit=no --option=ntlmssp_client:keyexchange=no --option=torture:quick=yes"]:
            if transport == "ncalrpc":
                env = "dc:local"
            else:
                env = "dc"
            plantestsuite_loadlist("samba4.rpc.echo on %s with %s and %s" % (transport, bindoptions, ntlmoptions), env, [smb4torture, "$LISTOPT", "%s:$SERVER[%s]" % (transport, bindoptions), ntlmoptions, '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.echo'])

plantestsuite_loadlist("samba4.rpc.echo on ncacn_np over smb2", "dc", [smb4torture, "$LISTOPT", 'ncacn_np:$SERVER[smb2]', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'rpc.echo'])

plantestsuite_loadlist("samba4.ntp.signd", "dc:local", [smb4torture, "$LISTOPT", 'ncacn_np:$SERVER', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', 'ntp.signd'])

# Tests against the NTVFS POSIX backend
ntvfsargs = ["--option=torture:sharedelay=10000", "--option=torture:oplocktimeout=3", "--option=torture:writetimeupdatedelay=50000"]

smb2 = smb4torture_testsuites("smb2.")
#The QFILEINFO-IPC test needs to be on ipc$
raw = filter(lambda x: "raw.qfileinfo.ipc" not in x, smb4torture_testsuites("raw."))
base = smb4torture_testsuites("base.")

for t in base + raw + smb2:
    plansmbtorturetestsuite(t, "dc", '//$SERVER/tmp -U$USERNAME%$PASSWORD' + " " + " ".join(ntvfsargs))

plansmbtorturetestsuite("raw.qfileinfo.ipc", "dc", '//$SERVER/ipc\$ -U$USERNAME%$PASSWORD')

for t in smb4torture_testsuites("rap."):
    plansmbtorturetestsuite(t, "dc", '//$SERVER/IPC\$ -U$USERNAME%$PASSWORD')

# Tests against the NTVFS CIFS backend
for t in base + raw:
    plantestsuite_loadlist("samba4.ntvfs.cifs.%s" % t, "dc", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/cifs', '-U$USERNAME%$PASSWORD'] + ntvfsargs + [t])

plansmbtorturetestsuite('echo.udp', 'dc:local', '//$SERVER/whatever')

# Local tests
for t in smb4torture_testsuites("local."):
    plansmbtorturetestsuite(t, "none", "ncalrpc:")

tdbtorture4 = binpath("tdbtorture")
if os.path.exists(tdbtorture4):
    plantestsuite("tdb.stress", "none", valgrindify(tdbtorture4))
else:
    skiptestsuite("tdb.stress", "Using system TDB, tdbtorture not available")

plansmbtorturetestsuite("drs.unit", "none", "ncalrpc:")

# Pidl tests
for f in sorted(os.listdir(os.path.join(samba4srcdir, "../pidl/tests"))):
    if f.endswith(".pl"):
        planperltestsuite("pidl.%s" % f[:-3], os.path.normpath(os.path.join(samba4srcdir, "../pidl/tests", f)))
planperltestsuite("selftest.samba4", os.path.normpath(os.path.join(samba4srcdir, "../selftest/test_samba4.pl")))

# Blackbox Tests:
# tests that interact directly with the command-line tools rather than using
# the API. These mainly test that the various command-line options of commands
# work correctly.

planpythontestsuite("none", "samba.tests.blackbox.ndrdump")
plantestsuite("samba4.blackbox.samba_tool(dc:local)", "dc:local", [os.path.join(samba4srcdir, "utils/tests/test_samba_tool.sh"),  '$SERVER', "$USERNAME", "$PASSWORD", "$DOMAIN"])
plantestsuite("samba4.blackbox.pkinit(dc:local)", "dc:local", [os.path.join(bbdir, "test_pkinit.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$REALM', '$DOMAIN', '$PREFIX', "aes256-cts-hmac-sha1-96", configuration])
plantestsuite("samba4.blackbox.kinit(dc:local)", "dc:local", [os.path.join(bbdir, "test_kinit.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$REALM', '$DOMAIN', '$PREFIX', "aes256-cts-hmac-sha1-96", configuration])
plantestsuite("samba4.blackbox.kinit(fl2000dc:local)", "fl2000dc:local", [os.path.join(bbdir, "test_kinit.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$REALM', '$DOMAIN', '$PREFIX', "arcfour-hmac-md5", configuration])
plantestsuite("samba4.blackbox.kinit(fl2008r2dc:local)", "fl2008r2dc:local", [os.path.join(bbdir, "test_kinit.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$REALM', '$DOMAIN', '$PREFIX', "aes256-cts-hmac-sha1-96", configuration])
plantestsuite("samba4.blackbox.ktpass(dc)", "dc", [os.path.join(bbdir, "test_ktpass.sh"), '$PREFIX'])
plantestsuite("samba4.blackbox.passwords(dc:local)", "dc:local", [os.path.join(bbdir, "test_passwords.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$REALM', '$DOMAIN', "$PREFIX"])
plantestsuite("samba4.blackbox.export.keytab(dc:local)", "dc:local", [os.path.join(bbdir, "test_export_keytab.sh"), '$SERVER', '$USERNAME', '$REALM', '$DOMAIN', "$PREFIX"])
plantestsuite("samba4.blackbox.cifsdd(dc)", "dc", [os.path.join(samba4srcdir, "client/tests/test_cifsdd.sh"), '$SERVER', '$USERNAME', '$PASSWORD', "$DOMAIN"])
plantestsuite("samba4.blackbox.nmblookup(dc)", "dc", [os.path.join(samba4srcdir, "utils/tests/test_nmblookup.sh"), '$NETBIOSNAME', '$NETBIOSALIAS', '$SERVER', '$SERVER_IP'])
plantestsuite("samba4.blackbox.nmblookup(member)", "member", [os.path.join(samba4srcdir, "utils/tests/test_nmblookup.sh"), '$NETBIOSNAME', '$NETBIOSALIAS', '$SERVER', '$SERVER_IP'])
plantestsuite("samba4.blackbox.locktest(dc)", "dc", [os.path.join(samba4srcdir, "torture/tests/test_locktest.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$DOMAIN', '$PREFIX'])
plantestsuite("samba4.blackbox.masktest", "dc", [os.path.join(samba4srcdir, "torture/tests/test_masktest.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$DOMAIN', '$PREFIX'])
plantestsuite("samba4.blackbox.gentest(dc)", "dc", [os.path.join(samba4srcdir, "torture/tests/test_gentest.sh"), '$SERVER', '$USERNAME', '$PASSWORD', '$DOMAIN', "$PREFIX"])
plantestsuite("samba4.blackbox.wbinfo(dc:local)", "dc:local", [os.path.join(samba4srcdir, "../nsswitch/tests/test_wbinfo.sh"), '$DOMAIN', '$USERNAME', '$PASSWORD', "dc"])
plantestsuite("samba4.blackbox.wbinfo(member:local)", "member:local", [os.path.join(samba4srcdir, "../nsswitch/tests/test_wbinfo.sh"), '$DOMAIN', '$DC_USERNAME', '$DC_PASSWORD', "member"])
plantestsuite("samba4.blackbox.chgdcpass(dc)", "dc", [os.path.join(bbdir, "test_chgdcpass.sh"), '$SERVER', "LOCALDC\$", '$REALM', '$DOMAIN', '$PREFIX', "aes256-cts-hmac-sha1-96", '$SELFTEST_PREFIX/dc'])

# Tests using the "Simple" NTVFS backend
for t in ["base.rw1"]:
    plantestsuite_loadlist("samba4.ntvfs.simple.%s" % t, "dc", [valgrindify(smb4torture), "$LISTOPT", "//$SERVER/simple", '-U$USERNAME%$PASSWORD', t])

# Domain Member Tests
plantestsuite_loadlist("samba4.rpc.echo against member server with local creds", "member", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_np:$NETBIOSNAME', '-U$NETBIOSNAME/$USERNAME%$PASSWORD', 'rpc.echo'])
plantestsuite_loadlist("samba4.rpc.echo against member server with domain creds", "member", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_np:$NETBIOSNAME', '-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD', 'rpc.echo'])
plantestsuite_loadlist("samba4.rpc.samr against member server with local creds", "member", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_np:$NETBIOSNAME', '-U$NETBIOSNAME/$USERNAME%$PASSWORD', "rpc.samr"])
plantestsuite_loadlist("samba4.rpc.samr.users against member server with local creds", "member", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_np:$NETBIOSNAME', '-U$NETBIOSNAME/$USERNAME%$PASSWORD', "rpc.samr.users"])
plantestsuite_loadlist("samba4.rpc.samr.passwords against member server with local creds", "member", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_np:$NETBIOSNAME', '-U$NETBIOSNAME/$USERNAME%$PASSWORD', "rpc.samr.passwords"])
plantestsuite("samba4.blackbox.smbclient against member server with local creds", "member", [os.path.join(samba4srcdir, "client/tests/test_smbclient.sh"), '$NETBIOSNAME', '$USERNAME', '$PASSWORD', '$NETBIOSNAME', '$PREFIX'])

# RPC Proxy
plantestsuite_loadlist("samba4.rpc.echo against rpc proxy with domain creds", "rpc_proxy", [valgrindify(smb4torture), "$LISTOPT", 'ncacn_ip_tcp:$NETBIOSNAME', '-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD', "rpc.echo"])

# Tests SMB signing
for mech in [
    "-k no",
    "-k no --option=usespnego=no",
    "-k no --option=gensec:spengo=no",
    "-k yes",
    "-k yes --option=gensec:fake_gssapi_krb5=yes --option=gensec:gssapi_krb5=no"]:
    for signing in ["--signing=on", "--signing=required"]:
        signoptions = "%s %s" % (mech, signing)
        name = "smb.signing on with %s" % signoptions
        plantestsuite_loadlist("samba4.%s" % name, "dc", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', signoptions, '-U$USERNAME%$PASSWORD', 'base.xcopy'])

for mech in [
    "-k no",
    "-k no --option=usespnego=no",
    "-k no --option=gensec:spengo=no",
    "-k yes",
    "-k yes --option=gensec:fake_gssapi_krb5=yes --option=gensec:gssapi_krb5=no"]:
    signoptions = "%s --signing=off" % mech
    name = "smb.signing on with %s" % signoptions
    plantestsuite_loadlist("samba4.%s domain-creds" % name, "member", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', signoptions, '-U$DC_USERNAME%$DC_PASSWORD', 'base.xcopy'])

for mech in [
    "-k no",
    "-k no --option=usespnego=no",
    "-k no --option=gensec:spengo=no"]:
    signoptions = "%s --signing=off" % mech
    name = "smb.signing on with %s" % signoptions
    plantestsuite_loadlist("samba4.%s local-creds" % name, "member", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', signoptions, '-U$NETBIOSNAME/$USERNAME%$PASSWORD', 'base.xcopy'])
plantestsuite_loadlist("samba4.smb.signing --signing=yes anon", "dc", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', '-k', 'no', '--signing=yes', '-U%', 'base.xcopy'])
plantestsuite_loadlist("samba4.smb.signing --signing=required anon", "dc", [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', '-k', 'no', '--signing=required', '-U%', 'base.xcopy'])
plantestsuite_loadlist("samba4.smb.signing --signing=no anon", "member",  [valgrindify(smb4torture), "$LISTOPT", '//$NETBIOSNAME/tmp', '-k', 'no', '--signing=no', '-U%', 'base.xcopy'])

nbt_tests = smb4torture_testsuites("nbt.")
for t in nbt_tests:
    plansmbtorturetestsuite(t, "dc", "//$SERVER/_none_ -U\"$USERNAME%$PASSWORD\"")

wb_opts = ["--option=\"torture:strict mode=no\"", "--option=\"torture:timelimit=1\"", "--option=\"torture:winbindd_separator=/\"", "--option=\"torture:winbindd_netbios_name=$SERVER\"", "--option=\"torture:winbindd_netbios_domain=$DOMAIN\""]

winbind_struct_tests = smb4torture_testsuites("winbind.struct")
winbind_ndr_tests = smb4torture_testsuites("winbind.ndr")
for env in ["dc", "member"]:
    for t in winbind_struct_tests:
        plansmbtorturetestsuite(t, env, "%s //_none_/_none_" % " ".join(wb_opts))

    for t in winbind_ndr_tests:
        plansmbtorturetestsuite(t, env, "%s //_none_/_none_" % " ".join(wb_opts))

nsstest4 = binpath("nsstest")
if os.path.exists(nsstest4):
    plantestsuite("samba4.nss.test using winbind(member)", "member", [valgrindify(nsstest4), os.path.join(samba4bindir, "shared/libnss_winbind.so")])
else:
    skiptestsuite("samba4.nss.test using winbind(member)", "nsstest not available")

subunitrun = valgrindify(python) + " " + os.path.join(samba4srcdir, "scripting/bin/subunitrun")
def plansambapythontestsuite(name, env, path, module, environ={}, extra_args=[]):
    environ = dict(environ)
    environ["PYTHONPATH"] = "$PYTHONPATH:" + path
    args = ["%s=%s" % item for item in environ.iteritems()]
    args += [subunitrun, "$LISTOPT", module]
    args += extra_args
    plantestsuite(name, env, args)


plansambapythontestsuite("ldb.python", "none", "%s/lib/ldb/tests/python/" % samba4srcdir, 'api')
planpythontestsuite("none", "samba.tests.credentials")
plantestsuite_idlist("samba.tests.gensec", "dc:local", [subunitrun, "$LISTOPT", '-U"$USERNAME%$PASSWORD"', "samba.tests.gensec"])
planpythontestsuite("none", "samba.tests.registry")
plansambapythontestsuite("tdb.python", "none", "%s/lib/tdb/python/tests" % srcdir(), 'simple')
planpythontestsuite("none", "samba.tests.auth")
planpythontestsuite("none", "samba.tests.security")
planpythontestsuite("none", "samba.tests.dcerpc.misc")
planpythontestsuite("none", "samba.tests.param")
planpythontestsuite("none", "samba.tests.upgrade")
planpythontestsuite("none", "samba.tests.core")
planpythontestsuite("none", "samba.tests.provision")
planpythontestsuite("none", "samba.tests.samba3")
planpythontestsuite("dc:local", "samba.tests.dcerpc.sam")
planpythontestsuite("dc:local", "samba.tests.dsdb")
planpythontestsuite("none", "samba.tests.netcmd")
planpythontestsuite("dc:local", "samba.tests.dcerpc.bare")
planpythontestsuite("dc:local", "samba.tests.dcerpc.unix")
planpythontestsuite("none", "samba.tests.dcerpc.rpc_talloc")
planpythontestsuite("none", "samba.tests.samdb")
planpythontestsuite("none", "samba.tests.hostconfig")
planpythontestsuite("none", "samba.tests.messaging")
planpythontestsuite("none", "samba.tests.samba3sam")
planpythontestsuite("none", "subunit")
planpythontestsuite("dc:local", "samba.tests.dcerpc.rpcecho")
plantestsuite_idlist("samba.tests.dcerpc.registry", "dc:local", [subunitrun, "$LISTOPT", '-U"$USERNAME%$PASSWORD"', "samba.tests.dcerpc.registry"])
plantestsuite("samba4.ldap.python(dc)", "dc", [python, os.path.join(samba4srcdir, "dsdb/tests/python/ldap.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
plantestsuite("samba4.tokengroups.python(dc)", "dc:local", [python, os.path.join(samba4srcdir, "dsdb/tests/python/token_group.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
plantestsuite("samba4.sam.python(dc)", "dc", [python, os.path.join(samba4srcdir, "dsdb/tests/python/sam.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
plansambapythontestsuite("samba4.schemaInfo.python(dc)", "dc", os.path.join(samba4srcdir, 'dsdb/tests/python'), 'dsdb_schema_info', extra_args=['-U"$DOMAIN/$DC_USERNAME%$DC_PASSWORD"'])
plantestsuite("samba4.urgent_replication.python(dc)", "dc", [python, os.path.join(samba4srcdir, "dsdb/tests/python/urgent_replication.py"), '$PREFIX_ABS/dc/private/sam.ldb'], allow_empty_output=True)
for env in ["dc", "fl2000dc", "fl2003dc", "fl2008r2dc"]:
    plantestsuite("samba4.ldap_schema.python(%s)" % env, env, [python, os.path.join(samba4srcdir, "dsdb/tests/python/ldap_schema.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
    plantestsuite("samba4.ldap.possibleInferiors.python(%s)" % env, env, [python, os.path.join(samba4srcdir, "dsdb/samdb/ldb_modules/tests/possibleinferiors.py"), "ldap://$SERVER", '-U"$USERNAME%$PASSWORD"', "-W", "$DOMAIN"])
    plantestsuite("samba4.ldap.secdesc.python(%s)" % env, env, [python, os.path.join(samba4srcdir, "dsdb/tests/python/sec_descriptor.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
    plantestsuite("samba4.ldap.acl.python(%s)" % env, env, [python, os.path.join(samba4srcdir, "dsdb/tests/python/acl.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
    if env != "fl2000dc":
        # This test makes excessively use of the "userPassword" attribute which
        # isn't available on DCs with Windows 2000 domain function level -
        # therefore skip it in that configuration
        plantestsuite("samba4.ldap.passwords.python(%s)" % env, env, [python, os.path.join(samba4srcdir, "dsdb/tests/python/passwords.py"), "$SERVER", '-U"$USERNAME%$PASSWORD"', "-W", "$DOMAIN"])
planpythontestsuite("dc:local", "samba.tests.upgradeprovisionneeddc")
planpythontestsuite("none", "samba.tests.upgradeprovision")
planpythontestsuite("none", "samba.tests.xattr")
planpythontestsuite("none", "samba.tests.ntacls")
plantestsuite("samba4.deletetest.python(dc)", "dc", ['PYTHONPATH="$PYTHONPATH:%s/lib/subunit/python:%s/lib/testtools"' % (srcdir(), srcdir()),
                                                     python, os.path.join(samba4srcdir, "dsdb/tests/python/deletetest.py"),
                                                     '$SERVER', '-U"$USERNAME%$PASSWORD"', '-W', '$DOMAIN'])
plansambapythontestsuite("samba4.policy.python", "none", "%s/lib/policy/tests/python" % samba4srcdir, 'bindings')
plantestsuite("samba4.blackbox.samba3dump", "none", [python, os.path.join(samba4srcdir, "scripting/bin/samba3dump"), os.path.join(samba4srcdir, "../testdata/samba3")], allow_empty_output=True)
plantestsuite("samba4.blackbox.upgrade", "none", ["rm -rf $PREFIX/upgrade;", python, os.path.join(samba4srcdir, "setup/upgrade_from_s3"), "--targetdir=$PREFIX/upgrade", os.path.normpath(os.path.join(samba4srcdir, "../testdata/samba3")), os.path.normpath(os.path.join(samba4srcdir, "../testdata/samba3/smb.conf"))], allow_empty_output=True)
plantestsuite("samba4.blackbox.provision.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_provision.sh"), '$PREFIX/provision'])
plantestsuite("samba4.blackbox.upgradeprovision.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_upgradeprovision.sh"), '$PREFIX/provision'])
plantestsuite("samba4.blackbox.setpassword.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_setpassword.sh"), '$PREFIX/provision'])
plantestsuite("samba4.blackbox.newuser.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_newuser.sh"), '$PREFIX/provision'])
plantestsuite("samba4.blackbox.group.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_group.sh"), '$PREFIX/provision'])
plantestsuite("samba4.blackbox.spn.py(dc:local)", "dc:local", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_spn.sh"), '$PREFIX/dc'])
plantestsuite("samba4.ldap.bind(dc)", "dc", [python, os.path.join(samba4srcdir, "auth/credentials/tests/bind.py"), '$SERVER', '-U"$USERNAME%$PASSWORD"'])

# DRS python tests
plansambapythontestsuite("samba4.blackbox.samba-tool.drs(vampire_dc)", "vampire_dc", os.path.join(samba4srcdir, 'scripting/python'), "samba.tests.blackbox.samba_tool_drs", environ={'DC1': '$DC_SERVER', 'DC2': '$VAMPIRE_DC_SERVER'}, extra_args=['-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD'])
plansambapythontestsuite("samba4.drs.replica_sync.python(vampire_dc)", "vampire_dc", os.path.join(samba4srcdir, 'torture/drs/python'), "replica_sync", environ={'DC1': '$DC_SERVER', 'DC2': '$VAMPIRE_DC_SERVER'}, extra_args=['-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD'])
plansambapythontestsuite("samba4.drs.delete_object.python(vampire_dc)", "vampire_dc", os.path.join(samba4srcdir, 'torture/drs/python'), "delete_object", environ={'DC1': '$DC_SERVER', 'DC2': '$VAMPIRE_DC_SERVER'}, extra_args=['-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD'])
plansambapythontestsuite("samba4.drs.fsmo.python(vampire_dc)", "vampire_dc", os.path.join(samba4srcdir, 'torture/drs/python'), "fsmo", environ={'DC1': "$DC_SERVER", 'DC2': "$VAMPIRE_DC_SERVER"}, extra_args=['-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD'])
plansambapythontestsuite("samba4.drs.repl_schema.python(vampire_dc)", "vampire_dc", os.path.join(samba4srcdir, 'torture/drs/python'), "repl_schema", environ={'DC1': "$DC_SERVER", 'DC2': '$VAMPIRE_DC_SERVER'}, extra_args=['-U$DOMAIN/$DC_USERNAME%$DC_PASSWORD'])

# This makes sure we test the rid allocation code
t = "rpc.samr.large-dc"
plantestsuite_loadlist("samba4.%s.one" % t, "vampire_dc", [valgrindify(smb4torture), "$LISTOPT", '$SERVER', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])
plantestsuite_loadlist("samba4.%s.two" % t, "vampire_dc", [valgrindify(smb4torture), "$LISTOPT", '$SERVER', '-U$USERNAME%$PASSWORD', '-W', '$DOMAIN', t])

# some RODC testing
plantestsuite_loadlist("samba4.rpc.echo", "rodc", [smb4torture, "$LISTOPT", 'ncacn_np:$SERVER', "-k", "yes", '-U$USERNAME%$PASSWORD', '-W' '$DOMAIN', 'rpc.echo'])
plantestsuite_loadlist("samba4.rpc.echo", "rodc:local", [smb4torture, "$LISTOPT", 'ncacn_np:$SERVER', "-k", "yes", '-P', '-W' '$DOMAIN', 'rpc.echo'])
plantestsuite("samba4.blackbox.provision-backend.py", "none", ["PYTHON=%s" % python, os.path.join(samba4srcdir, "setup/tests/blackbox_provision-backend.sh"), '$PREFIX/provision'])
