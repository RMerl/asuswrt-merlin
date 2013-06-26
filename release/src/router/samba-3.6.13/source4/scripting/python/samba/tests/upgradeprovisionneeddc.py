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

"""Tests for samba.upgradeprovision that need a DC."""

import os
import re
import shutil

from samba import param
from samba.credentials import Credentials
from samba.auth import system_session
from samba.provision import getpolicypath
from samba.upgradehelpers import (get_paths, get_ldbs,
                                 find_provision_key_parameters, identic_rename,
                                 updateOEMInfo, getOEMInfo, update_gpo,
                                 delta_update_basesamdb,
                                 update_dns_account_password,
                                 search_constructed_attrs_stored,
                                 increment_calculated_keyversion_number)
from samba.tests import env_loadparm, TestCaseInTempDir
from samba.tests.provision import create_dummy_secretsdb
import ldb


def dummymessage(a=None, b=None):
    pass

smb_conf_path = "%s/%s/%s" % (os.environ["SELFTEST_PREFIX"], "dc", "etc/smb.conf")

class UpgradeProvisionBasicLdbHelpersTestCase(TestCaseInTempDir):
    """Some simple tests for individual functions in the provisioning code.
    """

    def test_get_ldbs(self):
        paths = get_paths(param, None, smb_conf_path)
        creds = Credentials()
        lp = env_loadparm()
        creds.guess(lp)
        get_ldbs(paths, creds, system_session(), lp)

    def test_find_key_param(self):
        paths = get_paths(param, None, smb_conf_path)
        creds = Credentials()
        lp = env_loadparm()
        creds.guess(lp)
        rootdn = "dc=samba,dc=example,dc=com"
        ldbs = get_ldbs(paths, creds, system_session(), lp)
        names = find_provision_key_parameters(ldbs.sam, ldbs.secrets, ldbs.idmap,
                                                paths, smb_conf_path, lp)
        self.assertEquals(names.realm, "SAMBA.EXAMPLE.COM")
        self.assertEquals(str(names.rootdn).lower(), rootdn.lower())
        self.assertNotEquals(names.policyid_dc, None)
        self.assertNotEquals(names.ntdsguid, "")


class UpgradeProvisionWithLdbTestCase(TestCaseInTempDir):

    def _getEmptyDbName(self):
        return os.path.join(self.tempdir, "sam.ldb")

    def setUp(self):
        super(UpgradeProvisionWithLdbTestCase, self).setUp()
        paths = get_paths(param, None, smb_conf_path)
        self.creds = Credentials()
        self.lp = env_loadparm()
        self.creds.guess(self.lp)
        self.paths = paths
        self.ldbs = get_ldbs(paths, self.creds, system_session(), self.lp)
        self.names = find_provision_key_parameters(self.ldbs.sam,
                self.ldbs.secrets, self.ldbs.idmap, paths, smb_conf_path,
                self.lp)
        self.referencedb = create_dummy_secretsdb(
            os.path.join(self.tempdir, "ref.ldb"))

    def test_search_constructed_attrs_stored(self):
        hashAtt = search_constructed_attrs_stored(self.ldbs.sam,
                                                  self.names.rootdn,
                                                  ["msds-KeyVersionNumber"])
        self.assertFalse(hashAtt.has_key("msds-KeyVersionNumber"))

    def test_increment_calculated_keyversion_number(self):
        dn = "CN=Administrator,CN=Users,%s" % self.names.rootdn
        # We conctruct a simple hash for the user administrator
        hash = {}
        # And we want the version to be 140
        hash[dn.lower()] = 140

        increment_calculated_keyversion_number(self.ldbs.sam,
                                               self.names.rootdn,
                                               hash)
        self.assertEqual(self.ldbs.sam.get_attribute_replmetadata_version(dn,
                            "unicodePwd"),
                            140)
        # This function should not decrement the version
        hash[dn.lower()] = 130

        increment_calculated_keyversion_number(self.ldbs.sam,
                                               self.names.rootdn,
                                               hash)
        self.assertEqual(self.ldbs.sam.get_attribute_replmetadata_version(dn,
                            "unicodePwd"),
                            140)

    def test_identic_rename(self):
        rootdn = "DC=samba,DC=example,DC=com"

        guestDN = ldb.Dn(self.ldbs.sam, "CN=Guest,CN=Users,%s" % rootdn)
        identic_rename(self.ldbs.sam, guestDN)
        res = self.ldbs.sam.search(expression="(name=Guest)", base=rootdn,
                                scope=ldb.SCOPE_SUBTREE, attrs=["dn"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0]["dn"]), "CN=Guest,CN=Users,%s" % rootdn)

    def test_delta_update_basesamdb(self):
        dummysampath = self._getEmptyDbName()
        delta_update_basesamdb(self.paths.samdb, dummysampath,
                                self.creds, system_session(), self.lp,
                                dummymessage)

    def test_update_gpo_simple(self):
        dir = getpolicypath(self.paths.sysvol, self.names.dnsdomain,
                self.names.policyid)
        shutil.rmtree(dir)
        self.assertFalse(os.path.isdir(dir))
        update_gpo(self.paths, self.ldbs.sam, self.names, self.lp, dummymessage)
        self.assertTrue(os.path.isdir(dir))

    def test_update_gpo_acl(self):
        path = os.path.join(self.tempdir, "testupdategpo")
        save = self.paths.sysvol
        self.paths.sysvol = path
        os.mkdir(path)
        os.mkdir(os.path.join(path, self.names.dnsdomain))
        os.mkdir(os.path.join(os.path.join(path, self.names.dnsdomain),
            "Policies"))
        update_gpo(self.paths, self.ldbs.sam, self.names, self.lp, dummymessage)
        shutil.rmtree(path)
        self.paths.sysvol = save

    def test_getOEMInfo(self):
        realm = self.lp.get("realm")
        basedn = "DC=%s" % realm.replace(".", ", DC=")
        oem = getOEMInfo(self.ldbs.sam, basedn)
        self.assertNotEquals(oem, "")

    def test_update_dns_account(self):
        update_dns_account_password(self.ldbs.sam, self.ldbs.secrets, self.names)

    def test_updateOEMInfo(self):
        realm = self.lp.get("realm")
        basedn = "DC=%s" % realm.replace(".", ", DC=")
        oem = getOEMInfo(self.ldbs.sam, basedn)
        updateOEMInfo(self.ldbs.sam, basedn)
        oem2 = getOEMInfo(self.ldbs.sam, basedn)
        self.assertNotEquals(str(oem), str(oem2))
        self.assertTrue(re.match(".*upgrade to.*", str(oem2)))

    def tearDown(self):
        for name in ["ref.ldb", "secrets.ldb", "sam.ldb"]:
            path = os.path.join(self.tempdir, name)
            if os.path.exists(path):
                os.unlink(path)
        super(UpgradeProvisionWithLdbTestCase, self).tearDown()
