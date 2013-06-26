#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2008
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

"""Tests for samba.provision."""

import os
from samba.provision import setup_secretsdb, findnss, ProvisionPaths
import samba.tests
from samba.tests import env_loadparm, TestCase

def create_dummy_secretsdb(path, lp=None):
    """Create a dummy secrets database for use in tests.

    :param path: Path to store the secrets db
    :param lp: Optional loadparm context. A simple one will
        be generated if not specified.
    """
    if lp is None:
        lp = env_loadparm()
    paths = ProvisionPaths()
    paths.secrets = path
    paths.private_dir = os.path.dirname(path)
    paths.keytab = "no.keytab"
    paths.dns_keytab = "no.dns.keytab"
    secrets_ldb = setup_secretsdb(paths, None, None, lp=lp)
    secrets_ldb.transaction_commit()
    return secrets_ldb


class ProvisionTestCase(samba.tests.TestCaseInTempDir):
    """Some simple tests for individual functions in the provisioning code.
    """

    def test_setup_secretsdb(self):
        path = os.path.join(self.tempdir, "secrets.ldb")
        paths = ProvisionPaths()
        paths.secrets = path
        paths.private_dir = os.path.dirname(path)
        paths.keytab = "no.keytab"
        paths.dns_keytab = "no.dns.keytab"
        ldb = setup_secretsdb(paths, None, None, lp=env_loadparm())
        try:
            self.assertEquals("LSA Secrets",
                 ldb.searchone(basedn="CN=LSA Secrets", attribute="CN"))
        finally:
            del ldb
            os.unlink(path)
            

class FindNssTests(TestCase):
    """Test findnss() function."""

    def test_nothing(self):
        def x(y):
            raise KeyError
        self.assertRaises(KeyError, findnss, x, [])

    def test_first(self):
        self.assertEquals("bla", findnss(lambda x: "bla", ["bla"]))

    def test_skip_first(self):
        def x(y):
            if y != "bla":
                raise KeyError
            return "ha"
        self.assertEquals("ha", findnss(x, ["bloe", "bla"]))


class Disabled(object):

    def test_setup_templatesdb(self):
        raise NotImplementedError(self.test_setup_templatesdb)

    def test_setup_registry(self):
        raise NotImplementedError(self.test_setup_registry)

    def test_setup_samdb_rootdse(self):
        raise NotImplementedError(self.test_setup_samdb_rootdse)

    def test_setup_samdb_partitions(self):
        raise NotImplementedError(self.test_setup_samdb_partitions)

    def test_create_phpldapadmin_config(self):
        raise NotImplementedError(self.test_create_phpldapadmin_config)

    def test_provision_dns(self):
        raise NotImplementedError(self.test_provision_dns)

    def test_provision_ldapbase(self):
        raise NotImplementedError(self.test_provision_ldapbase)

    def test_provision_guess(self):
        raise NotImplementedError(self.test_provision_guess)

    def test_join_domain(self):
        raise NotImplementedError(self.test_join_domain)

    def test_vampire(self):
        raise NotImplementedError(self.test_vampire)


