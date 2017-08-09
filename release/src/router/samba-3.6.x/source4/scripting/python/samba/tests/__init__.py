#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2010
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""Samba Python tests."""

import os
import ldb
import samba
import samba.auth
from samba import param
from samba.samdb import SamDB
import subprocess
import tempfile

# Other modules import these two classes from here, for convenience:
from testtools.testcase import (
    TestCase as TesttoolsTestCase,
    TestSkipped,
    )


class TestCase(TesttoolsTestCase):
    """A Samba test case."""

    def setUp(self):
        super(TestCase, self).setUp()
        test_debug_level = os.getenv("TEST_DEBUG_LEVEL")
        if test_debug_level is not None:
            test_debug_level = int(test_debug_level)
            self._old_debug_level = samba.get_debug_level()
            samba.set_debug_level(test_debug_level)
            self.addCleanup(samba.set_debug_level, test_debug_level)

    def get_loadparm(self):
        return env_loadparm()

    def get_credentials(self):
        return cmdline_credentials


class LdbTestCase(TesttoolsTestCase):
    """Trivial test case for running tests against a LDB."""

    def setUp(self):
        super(LdbTestCase, self).setUp()
        self.filename = os.tempnam()
        self.ldb = samba.Ldb(self.filename)

    def set_modules(self, modules=[]):
        """Change the modules for this Ldb."""
        m = ldb.Message()
        m.dn = ldb.Dn(self.ldb, "@MODULES")
        m["@LIST"] = ",".join(modules)
        self.ldb.add(m)
        self.ldb = samba.Ldb(self.filename)


class TestCaseInTempDir(TestCase):

    def setUp(self):
        super(TestCaseInTempDir, self).setUp()
        self.tempdir = tempfile.mkdtemp()

    def tearDown(self):
        super(TestCaseInTempDir, self).tearDown()
        self.assertEquals([], os.listdir(self.tempdir))
        os.rmdir(self.tempdir)


def env_loadparm():
    lp = param.LoadParm()
    try:
        lp.load(os.environ["SMB_CONF_PATH"])
    except KeyError:
        raise Exception("SMB_CONF_PATH not set")
    return lp


def env_get_var_value(var_name):
    """Returns value for variable in os.environ

    Function throws AssertionError if variable is defined.
    Unit-test based python tests require certain input params
    to be set in environment, otherwise they can't be run
    """
    assert var_name in os.environ.keys(), "Please supply %s in environment" % var_name
    return os.environ[var_name]


cmdline_credentials = None

class RpcInterfaceTestCase(TestCase):
    """DCE/RPC Test case."""


class ValidNetbiosNameTests(TestCase):

    def test_valid(self):
        self.assertTrue(samba.valid_netbios_name("FOO"))

    def test_too_long(self):
        self.assertFalse(samba.valid_netbios_name("FOO"*10))

    def test_invalid_characters(self):
        self.assertFalse(samba.valid_netbios_name("*BLA"))


class BlackboxProcessError(subprocess.CalledProcessError):
    """This exception is raised when a process run by check_output() returns
    a non-zero exit status. Exception instance should contain
    the exact exit code (S.returncode), command line (S.cmd),
    process output (S.stdout) and process error stream (S.stderr)"""
    def __init__(self, returncode, cmd, stdout, stderr):
        super(BlackboxProcessError, self).__init__(returncode, cmd)
        self.stdout = stdout
        self.stderr = stderr
    def __str__(self):
        return "Command '%s'; exit status %d; stdout: '%s'; stderr: '%s'" % (self.cmd, self.returncode,
                                                                             self.stdout, self.stderr)

class BlackboxTestCase(TestCase):
    """Base test case for blackbox tests."""

    def _make_cmdline(self, line):
        bindir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../../bin"))
        parts = line.split(" ")
        if os.path.exists(os.path.join(bindir, parts[0])):
            parts[0] = os.path.join(bindir, parts[0])
        line = " ".join(parts)
        return line

    def check_run(self, line):
        line = self._make_cmdline(line)
        subprocess.check_call(line, shell=True)

    def check_output(self, line):
        line = self._make_cmdline(line)
        p = subprocess.Popen(line, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, close_fds=True)
        retcode = p.wait()
        if retcode:
            raise BlackboxProcessError(retcode, line, p.stdout.read(), p.stderr.read())
        return p.stdout.read()

def connect_samdb(samdb_url, lp=None, session_info=None, credentials=None,
                  flags=0, ldb_options=None, ldap_only=False):
    """Create SamDB instance and connects to samdb_url database.

    :param samdb_url: Url for database to connect to.
    :param lp: Optional loadparm object
    :param session_info: Optional session information
    :param credentials: Optional credentials, defaults to anonymous.
    :param flags: Optional LDB flags
    :param ldap_only: If set, only remote LDAP connection will be created.

    Added value for tests is that we have a shorthand function
    to make proper URL for ldb.connect() while using default
    parameters for connection based on test environment
    """
    samdb_url = samdb_url.lower()
    if not "://" in samdb_url:
        if not ldap_only and os.path.isfile(samdb_url):
            samdb_url = "tdb://%s" % samdb_url
        else:
            samdb_url = "ldap://%s" % samdb_url
    # use 'paged_search' module when connecting remotely
    if samdb_url.startswith("ldap://"):
        ldb_options = ["modules:paged_searches"]
    elif ldap_only:
        raise AssertionError("Trying to connect to %s while remote "
                             "connection is required" % samdb_url)

    # set defaults for test environment
    if lp is None:
        lp = env_loadparm()
    if session_info is None:
        session_info = samba.auth.system_session(lp)
    if credentials is None:
        credentials = cmdline_credentials

    return SamDB(url=samdb_url,
                 lp=lp,
                 session_info=session_info,
                 credentials=credentials,
                 flags=flags,
                 options=ldb_options)


def connect_samdb_ex(samdb_url, lp=None, session_info=None, credentials=None,
                     flags=0, ldb_options=None, ldap_only=False):
    """Connects to samdb_url database

    :param samdb_url: Url for database to connect to.
    :param lp: Optional loadparm object
    :param session_info: Optional session information
    :param credentials: Optional credentials, defaults to anonymous.
    :param flags: Optional LDB flags
    :param ldap_only: If set, only remote LDAP connection will be created.
    :return: (sam_db_connection, rootDse_record) tuple
    """
    sam_db = connect_samdb(samdb_url, lp, session_info, credentials,
                           flags, ldb_options, ldap_only)
    # fetch RootDse
    res = sam_db.search(base="", expression="", scope=ldb.SCOPE_BASE,
                        attrs=["*"])
    return (sam_db, res[0])


def delete_force(samdb, dn):
    try:
        samdb.delete(dn)
    except ldb.LdbError, (num, _):
        assert(num == ldb.ERR_NO_SUCH_OBJECT)
