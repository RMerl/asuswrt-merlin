#!/usr/bin/python
# This script generates a list of testsuites that should be run as part of 
# the Samba 3 test suite.

# The output of this script is parsed by selftest.pl, which then decides 
# which of the tests to actually run. It will, for example, skip all tests 
# listed in selftest/skip or only run a subset during "make quicktest".

# The idea is that this script outputs all of the tests of Samba 3, not 
# just those that are known to pass, and list those that should be skipped 
# or are known to fail in selftest/skip or selftest/samba4-knownfail. This makes it 
# very easy to see what functionality is still missing in Samba 3 and makes 
# it possible to run the testsuite against other servers, such as Samba 4 or 
# Windows that have a different set of features.

# The syntax for a testsuite is "-- TEST --" on a single line, followed 
# by the name of the test, the environment it needs and the command to run, all 
# three separated by newlines. All other lines in the output are considered 
# comments.

import os, sys
sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(__file__), "../../selftest")))
from selftesthelpers import *
import subprocess

smb4torture = binpath("smbtorture4")
samba3srcdir = srcdir() + "/source3"
configuration = "--configfile=$SMB_CONF_PATH"
scriptdir=os.path.join(samba3srcdir, "../script/tests")

torture_options = [configuration, "--maximum-runtime=$SELFTEST_MAXTIME", 
                   "--target=$SELFTEST_TARGET", "--basedir=$SELFTEST_TMPDIR",
                   '--option="torture:winbindd_netbios_name=$SERVER"',
                   '--option="torture:winbindd_netbios_domain=$DOMAIN"', 
                   '--option=torture:sharedelay=100000']

if not os.getenv("SELFTEST_VERBOSE"):
    torture_options.append("--option=torture:progress=no")
torture_options.append("--format=subunit")
if os.getenv("SELFTEST_QUICK"):
    torture_options.append("--option=torture:quick=yes")
smb4torture += " " + " ".join(torture_options)

def plansmbtorturetestsuite(name, env, options, description=''):
    modname = "samba3.posix_s3.%s %s" % (name, description)
    cmdline = "%s $LISTOPT %s %s" % (valgrindify(smb4torture), options, name)
    plantestsuite_loadlist(modname, env, cmdline)

plantestsuite("samba3.blackbox.success", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_success.sh")])
plantestsuite("samba3.blackbox.failure", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_failure.sh")])

plantestsuite("samba3.local_s3", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_local_s3.sh")])

tests=[ "FDPASS", "LOCK1", "LOCK2", "LOCK3", "LOCK4", "LOCK5", "LOCK6", "LOCK7", "LOCK9",
        "UNLINK", "BROWSE", "ATTR", "TRANS2", "TORTURE",
        "OPLOCK1", "OPLOCK2", "OPLOCK3", "OPLOCK4", "STREAMERROR",
        "DIR", "DIR1", "DIR-CREATETIME", "TCON", "TCONDEV", "RW1", "RW2", "RW3", "RW-SIGNING",
        "OPEN", "XCOPY", "RENAME", "DELETE", "DELETE-LN", "PROPERTIES", "W2K",
        "TCON2", "IOCTL", "CHKPATH", "FDSESS", "LOCAL-SUBSTITUTE", "CHAIN1",
        "GETADDRINFO", "POSIX", "UID-REGRESSION-TEST", "SHORTNAME-TEST",
        "LOCAL-BASE64", "LOCAL-GENCACHE", "POSIX-APPEND",
        "LOCAL-string_to_sid" ]

for t in tests:
    plantestsuite("samba3.smbtorture_s3.plain(dc).%s" % t, "s3dc", [os.path.join(samba3srcdir, "script/tests/test_smbtorture_s3.sh"), t, '//$SERVER_IP/tmp', '$USERNAME', '$PASSWORD', "", "-l $LOCAL_PATH"])
    plantestsuite("samba3.smbtorture_s3.crypt(dc).%s" % t, "s3dc", [os.path.join(samba3srcdir, "script/tests/test_smbtorture_s3.sh"), t, '//$SERVER_IP/tmp', '$USERNAME', '$PASSWORD', "-e", "-l $LOCAL_PATH"])

tests=["--ping", "--separator",
       "--own-domain",
       "--all-domains",
       "--trusted-domains",
       "--domain-info=BUILTIN",
       "--domain-info=$DOMAIN",
       "--online-status",
       "--online-status --domain=BUILTIN",
       "--online-status --domain=$DOMAIN",
       "--check-secret --domain=$DOMAIN",
       "--change-secret --domain=$DOMAIN",
       "--check-secret --domain=$DOMAIN",
       "--online-status --domain=$DOMAIN",
       #Didn't pass yet# "--domain-users",
       "--domain-groups",
       "--name-to-sid=$USERNAME",
       "--name-to-sid=$DOMAIN\\\\$USERNAME",
     #Didn't pass yet# "--user-info=$USERNAME",
       "--user-groups=$DOMAIN\\\\$USERNAME",
       "--allocate-uid",
       "--allocate-gid"]

for t in tests:
    plantestsuite("samba3.wbinfo_s3.(s3dc:local).%s" % t, "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_wbinfo_s3.sh"), t])
    plantestsuite("samba3.wbinfo_s3.(member:local).%s" % t, "member:local", [os.path.join(samba3srcdir, "script/tests/test_wbinfo_s3.sh"), t])

plantestsuite(
    "samba3.wbinfo_sids2xids.(member:local)", "member:local",
    [os.path.join(samba3srcdir, "script/tests/test_wbinfo_sids2xids.sh")])

plantestsuite("samba3.ntlm_auth.(s3dc:local)", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_ntlm_auth_s3.sh"), valgrindify(python), samba3srcdir, configuration])

for env in ["s3dc", "member"]:
    plantestsuite("samba3.blackbox.smbclient_auth.plain (%s)" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_auth.sh"), '$SERVER', '$SERVER_IP', '$DC_USERNAME', '$DC_PASSWORD', configuration])

for env in ["secserver"]:
    plantestsuite("samba3.blackbox.smbclient_auth.plain (%s) domain creds" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_auth.sh"), '$SERVER', '$SERVER_IP', '$DOMAIN\\\\$DC_USERNAME', '$DC_PASSWORD', configuration + " --option=clientntlmv2auth=no"])

for env in ["member"]:
    plantestsuite("samba3.blackbox.smbclient_auth.plain (%s) member creds" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_auth.sh"), '$SERVER', '$SERVER_IP', '$SERVER\\\\$USERNAME', '$PASSWORD', configuration])

for env in ["secshare", "secserver"]:
    plantestsuite("samba3.blackbox.smbclient_auth.plain (%s) local creds" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_auth.sh"), '$SERVER', '$SERVER_IP', '$USERNAME', '$PASSWORD', configuration + " --option=clientntlmv2auth=no --option=clientlanmanauth=yes"])

# plain
for env in ["s3dc"]:
    plantestsuite("samba3.blackbox.smbclient_s3.plain (%s)" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_s3.sh"), '$SERVER', '$SERVER_IP', '$DC_USERNAME', '$DC_PASSWORD', '$USERID', '$LOCAL_PATH', '$PREFIX', configuration])

for env in ["member"]:
    plantestsuite("samba3.blackbox.smbclient_s3.plain (%s) member creds" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_s3.sh"), '$SERVER', '$SERVER_IP', '$SERVER\\\\$USERNAME', '$PASSWORD', '$USERID', '$LOCAL_PATH', '$PREFIX', configuration])

for env in ["s3dc"]:
    plantestsuite("samba3.blackbox.smbclient_s3.sign (%s)" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_s3.sh"), '$SERVER', '$SERVER_IP', '$DC_USERNAME', '$DC_PASSWORD', '$USERID', '$LOCAL_PATH', '$PREFIX', configuration, "--signing=required"])

for env in ["member"]:
    plantestsuite("samba3.blackbox.smbclient_s3.sign (%s) member creds" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_s3.sh"), '$SERVER', '$SERVER_IP', '$SERVER\\\\$USERNAME', '$PASSWORD', '$USERID', '$LOCAL_PATH', '$PREFIX', configuration, "--signing=required"])

# encrypted
for env in ["s3dc"]:
    plantestsuite("samba3.blackbox.smbclient_s3.crypt (%s)" % env, env, [os.path.join(samba3srcdir, "script/tests/test_smbclient_s3.sh"), '$SERVER', '$SERVER_IP', '$USERNAME', '$PASSWORD', '$USERID', '$LOCAL_PATH', '$PREFIX', configuration, "-e"])

#TODO encrypted against member, with member creds, and with DC creds
plantestsuite("samba3.blackbox.net.misc", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_net_misc.sh"),
                                                       scriptdir, "$SMB_CONF_PATH", configuration])
plantestsuite("samba3.blackbox.net.local.registry", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_net_registry.sh"),
                                                       scriptdir, "$SMB_CONF_PATH", configuration])
plantestsuite("samba3.blackbox.net.rpc.registry", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_net_registry.sh"),
                                                       scriptdir, "$SMB_CONF_PATH", configuration, 'rpc'])

plantestsuite("samba3.blackbox.net.local.registry.roundtrip", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_net_registry_roundtrip.sh"),
                                                       scriptdir, "$SMB_CONF_PATH", configuration])
plantestsuite("samba3.blackbox.testparm", "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_testparm_s3.sh"),
                                                       "$LOCAL_PATH"])

plantestsuite(
    "samba3.pthreadpool", "s3dc",
    [os.path.join(samba3srcdir, "script/tests/test_pthreadpool.sh")])

#smbtorture4 tests

base = ["base.attr", "base.charset", "base.chkpath", "base.defer_open", "base.delaywrite", "base.delete",
        "base.deny1", "base.deny2", "base.deny3", "base.denydos", "base.dir1", "base.dir2",
        "base.disconnect", "base.fdpass", "base.lock",
        "base.mangle", "base.negnowait", "base.ntdeny1",
        "base.ntdeny2", "base.open", "base.openattr", "base.properties", "base.rename", "base.rw1",
        "base.secleak", "base.tcon", "base.tcondev", "base.trans2", "base.unlink", "base.vuid",
        "base.xcopy", "base.samba3error"]

raw = ["raw.acls", "raw.chkpath", "raw.close", "raw.composite", "raw.context", "raw.eas",
       "raw.ioctl", "raw.lock", "raw.mkdir", "raw.mux", "raw.notify", "raw.open", "raw.oplock"
       "raw.qfileinfo", "raw.qfsinfo", "raw.read", "raw.rename", "raw.search", "raw.seek",
       "raw.sfileinfo.base", "raw.sfileinfo.bug", "raw.streams", "raw.unlink", "raw.write",
       "raw.samba3hide", "raw.samba3badpath", "raw.sfileinfo.rename",
       "raw.samba3caseinsensitive", "raw.samba3posixtimedlock",
       "raw.samba3rootdirfid", "raw.sfileinfo.end-of-file"]

smb2 = ["smb2.lock", "smb2.read", "smb2.compound", "smb2.connect", "smb2.scan", "smb2.scanfind",
        "smb2.bench-oplock", "smb2.oplock"]

rpc = ["rpc.authcontext", "rpc.samba3.bind", "rpc.samba3.srvsvc", "rpc.samba3.sharesec",
       "rpc.samba3.spoolss", "rpc.samba3.wkssvc", "rpc.samba3.winreg",
       "rpc.samba3.getaliasmembership-0",
       "rpc.samba3.netlogon", "rpc.samba3.sessionkey", "rpc.samba3.getusername",
       "rpc.svcctl", "rpc.ntsvcs", "rpc.winreg", "rpc.eventlog",
       "rpc.spoolss.printserver", "rpc.spoolss.win", "rpc.spoolss.notify", "rpc.spoolss.printer",
       "rpc.spoolss.driver",
       "rpc.lsa-getuser", "rpc.lsa.lookupsids", "rpc.lsa.lookupnames",
       "rpc.lsa.privileges", 
       "rpc.samr", "rpc.samr.users", "rpc.samr.users.privileges", "rpc.samr.passwords",
       "rpc.samr.passwords.pwdlastset", "rpc.samr.large-dc", "rpc.samr.machine.auth",
       "rpc.netlogon.admin",
       "rpc.schannel", "rpc.schannel2", "rpc.bench-schannel1", "rpc.join", "rpc.bind", "rpc.epmapper"]

local = ["local.nss-wrapper", "local.ndr"]

winbind = ["winbind.struct", "winbind.wbclient"]

rap = ["rap.basic", "rap.rpc", "rap.printing", "rap.sam"]

unix = ["unix.info2", "unix.whoami"]

nbt = ["nbt.dgram" ]

tests= base + raw + smb2 + rpc + unix + local + winbind + rap + nbt

sub = subprocess.Popen("%s --version 2> /dev/null" % smb4torture, stdout=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)
sub.communicate("")

if sub.returncode == 0:
    for t in tests:
        if t == "base.delaywrite":
            plansmbtorturetestsuite(t, "s3dc", '//$SERVER_IP/tmp -U$USERNAME%$PASSWORD --maximum-runtime=900')
        elif t == "unix.whoami":
            plansmbtorturetestsuite(t, "s3dc", '//$SERVER_IP/tmpguest -U$USERNAME%$PASSWORD')
        elif t == "raw.samba3posixtimedlock":
            plansmbtorturetestsuite(t, "s3dc", '//$SERVER_IP/tmpguest -U$USERNAME%$PASSWORD --option=torture:localdir=$SELFTEST_PREFIX/dc/share')
        elif t == "rpc.samr.passwords.validate":
            plansmbtorturetestsuite(t, "s3dc", 'ncacn_np:$SERVER_IP[seal] -U$USERNAME%$PASSWORD', 'over ncacn_np ')
        else:
            plansmbtorturetestsuite(t, "s3dc", '//$SERVER_IP/tmp -U$USERNAME%$PASSWORD')

        if t == "raw.chkpath":
            plansmbtorturetestsuite(t, "s3dc", '//$SERVER_IP/tmpcase -U$USERNAME%$PASSWORD')

    test = 'rpc.lsa.lookupsids'
    auth_options = ["", "ntlm", "spnego", "spnego,ntlm" ]
    signseal_options = ["", ",connect", ",sign", ",seal"]
    smb_options = ["", ",smb2"]
    endianness_options = ["", ",bigendian"]
    for z in smb_options:
        for e in endianness_options:
            for a in auth_options:
                for s in signseal_options:
                    binding_string = "ncacn_np:$SERVER_IP[%s%s%s%s]" % (a, s, z, e)
                    options = binding_string + " -U$USERNAME%$PASSWORD"
                    plansmbtorturetestsuite(test, "s3dc", options, 'over ncacn_np with [%s%s%s%s] ' % (a, s, z, e))
                    plantestsuite("samba3.blackbox.rpcclient over ncacn_np with [%s%s%s%s] " % (a, s, z, e), "s3dc:local", [os.path.join(samba3srcdir, "script/tests/test_rpcclient.sh"),
                                                                 "none", options, configuration])

    for e in endianness_options:
        for a in auth_options:
            for s in signseal_options:
                binding_string = "ncacn_ip_tcp:$SERVER_IP[%s%s%s]" % (a, s, e)
                options = binding_string + " -U$USERNAME%$PASSWORD"
                plansmbtorturetestsuite(test, "s3dc", options, 'over ncacn_ip_tcp with [%s%s%s] ' % (a, s, e))
