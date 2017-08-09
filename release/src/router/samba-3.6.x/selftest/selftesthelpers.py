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

import os
import subprocess

def srcdir():
    return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

def source4dir():
    return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../source4"))

def bindir():
    return os.path.normpath(os.path.join(os.getenv("BUILDDIR", "."), "bin"))

def binpath(name):
    return os.path.join(bindir(), "%s%s" % (name, os.getenv("EXEEXT", "")))

perl = os.getenv("PERL", "perl")
perl = perl.split()

if subprocess.call(perl + ["-e", "eval require Test::More;"]) == 0:
    has_perl_test_more = True
else:
    has_perl_test_more = False

try:
    from subunit.run import TestProgram
except ImportError:
    has_system_subunit_run = False
else:
    has_system_subunit_run = True

python = os.getenv("PYTHON", "python")

#Set a default value, overridden if we find a working one on the system
tap2subunit = "PYTHONPATH=%s/lib/subunit/python:%s/lib/testtools %s %s/lib/subunit/filters/tap2subunit" % (srcdir(), srcdir(), python, srcdir())

sub = subprocess.Popen("tap2subunit 2> /dev/null", stdout=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)
sub.communicate("")

if sub.returncode == 0:
    cmd = "echo -ne \"1..1\nok 1 # skip doesn't seem to work yet\n\" | tap2subunit 2> /dev/null | grep skip"
    sub = subprocess.Popen(cmd, stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    if sub.returncode == 0:
        tap2subunit = "tap2subunit"

def valgrindify(cmdline):
    """Run a command under valgrind, if $VALGRIND was set."""
    valgrind = os.getenv("VALGRIND")
    if valgrind is None:
        return cmdline
    return valgrind + " " + cmdline


def plantestsuite(name, env, cmdline, allow_empty_output=False):
    """Plan a test suite.

    :param name: Testsuite name
    :param env: Environment to run the testsuite in
    :param cmdline: Command line to run
    """
    print "-- TEST --"
    print name
    print env
    if isinstance(cmdline, list):
        cmdline = " ".join(cmdline)
    filter_subunit_args = []
    if not allow_empty_output:
        filter_subunit_args.append("--fail-on-empty")
    if "$LISTOPT" in cmdline:
        filter_subunit_args.append("$LISTOPT")
    print "%s 2>&1 | %s/selftest/filter-subunit %s --prefix=\"%s.\"" % (cmdline,
                                                                        srcdir(),
                                                                        " ".join(filter_subunit_args),
                                                                        name)
    if allow_empty_output:
        print "WARNING: allowing empty subunit output from %s" % name


def add_prefix(prefix, support_list=False):
    if support_list:
        listopt = "$LISTOPT "
    else:
        listopt = ""
    return "%s/selftest/filter-subunit %s--fail-on-empty --prefix=\"%s.\"" % (srcdir(), listopt, prefix)


def plantestsuite_loadlist(name, env, cmdline):
    print "-- TEST-LOADLIST --"
    if env == "none":
        fullname = name
    else:
        fullname = "%s(%s)" % (name, env)
    print fullname
    print env
    if isinstance(cmdline, list):
        cmdline = " ".join(cmdline)
    support_list = ("$LISTOPT" in cmdline)
    print "%s $LOADLIST 2>&1 | %s" % (cmdline, add_prefix(name, support_list))


def plantestsuite_idlist(name, env, cmdline):
    print "-- TEST-IDLIST --"
    print name
    print env
    if isinstance(cmdline, list):
        cmdline = " ".join(cmdline)
    print cmdline


def skiptestsuite(name, reason):
    """Indicate that a testsuite was skipped.

    :param name: Test suite name
    :param reason: Reason the test suite was skipped
    """
    # FIXME: Report this using subunit, but re-adjust the testsuite count somehow
    print "skipping %s (%s)" % (name, reason)


def planperltestsuite(name, path):
    """Run a perl test suite.

    :param name: Name of the test suite
    :param path: Path to the test runner
    """
    if has_perl_test_more:
        plantestsuite(name, "none", "%s %s | %s" % (" ".join(perl), path, tap2subunit))
    else:
        skiptestsuite(name, "Test::More not available")


def planpythontestsuite(env, module):
    if has_system_subunit_run:
        plantestsuite_idlist(module, env, [python, "-m", "subunit.run", "$LISTOPT", module])
    else:
        plantestsuite_idlist(module, env, "PYTHONPATH=$PYTHONPATH:%s/lib/subunit/python:%s/lib/testtools %s -m subunit.run $LISTOPT %s" % (srcdir(), srcdir(), python, module))


