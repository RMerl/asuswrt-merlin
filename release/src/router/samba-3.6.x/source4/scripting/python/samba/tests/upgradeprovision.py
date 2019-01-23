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

"""Tests for samba.upgradeprovision."""

import os
from samba.upgradehelpers import  (usn_in_range, dn_sort,
                                  get_diff_sddls, update_secrets,
                                  construct_existor_expr)

from samba.tests.provision import create_dummy_secretsdb
from samba.tests import TestCaseInTempDir
from samba import Ldb
from ldb import SCOPE_SUBTREE
import samba.tests

def dummymessage(a=None, b=None):
    pass


class UpgradeProvisionTestCase(TestCaseInTempDir):
    """Some simple tests for individual functions in the provisioning code.
    """
    def test_usn_in_range(self):
        range = [5, 25, 35, 55]

        vals = [3, 26, 56]

        for v in vals:
            self.assertFalse(usn_in_range(v, range))

        vals = [5, 20, 25, 35, 36]

        for v in vals:
            self.assertTrue(usn_in_range(v, range))

    def test_dn_sort(self):
        # higher level comes after lower even if lexicographicaly closer
        # ie dc=tata,dc=toto (2 levels), comes after dc=toto
        # even if dc=toto is lexicographicaly after dc=tata, dc=toto
        self.assertEquals(dn_sort("dc=tata,dc=toto", "dc=toto"), 1)
        self.assertEquals(dn_sort("dc=zata", "dc=tata"), 1)
        self.assertEquals(dn_sort("dc=toto,dc=tata",
                                    "cn=foo,dc=toto,dc=tata"), -1)
        self.assertEquals(dn_sort("cn=bar, dc=toto,dc=tata",
                                    "cn=foo, dc=toto,dc=tata"), -1)

    def test_get_diff_sddl(self):
        sddl = "O:SAG:DUD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)S:AI(AU;CIIDSA;WP;;;WD)"
        sddl1 = "O:SAG:DUD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)S:AI(AU;CIIDSA;WP;;;WD)"
        sddl2 = "O:BAG:DUD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)S:AI(AU;CIIDSA;WP;;;WD)"
        sddl3 = "O:SAG:BAD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)S:AI(AU;CIIDSA;WP;;;WD)"
        sddl4 = "O:SAG:DUD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;BA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)S:AI(AU;CIIDSA;WP;;;WD)"
        sddl5 = "O:SAG:DUD:AI(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA)\
(A;CIID;RP LCLORC;;;AU)(A;CIID;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)"

        self.assertEquals(get_diff_sddls(sddl, sddl1), "")
        txt = get_diff_sddls(sddl, sddl2)
        self.assertEquals(txt, "\tOwner mismatch: SA (in ref) BA(in current)\n")
        txt = get_diff_sddls(sddl, sddl3)
        self.assertEquals(txt, "\tGroup mismatch: DU (in ref) BA(in current)\n")
        txt = get_diff_sddls(sddl, sddl4)
        txtmsg = "\tPart dacl is different between reference and current here\
 is the detail:\n\t\t(A;CIID;RPWPCRCCLCLORCWOWDSW;;;BA) ACE is not present in\
 the reference\n\t\t(A;CIID;RPWPCRCCLCLORCWOWDSW;;;SA) ACE is not present in\
 the current\n"
        self.assertEquals(txt, txtmsg)
        txt = get_diff_sddls(sddl, sddl5)
        self.assertEquals(txt, "\tCurrent ACL hasn't a sacl part\n")

    def test_construct_existor_expr(self):
        res = construct_existor_expr([])
        self.assertEquals(res, "")

        res = construct_existor_expr(["foo"])
        self.assertEquals(res, "(|(foo=*))")

        res = construct_existor_expr(["foo", "bar"])
        self.assertEquals(res, "(|(foo=*)(bar=*))")


class UpdateSecretsTests(samba.tests.TestCaseInTempDir):

    def setUp(self):
        super(UpdateSecretsTests, self).setUp()
        self.referencedb = create_dummy_secretsdb(
            os.path.join(self.tempdir, "ref.ldb"))

    def _getEmptyDb(self):
        return Ldb(os.path.join(self.tempdir, "secrets.ldb"))

    def _getCurrentFormatDb(self):
        return create_dummy_secretsdb(
            os.path.join(self.tempdir, "secrets.ldb"))

    def test_trivial(self):
        # Test that updating an already up-to-date secretsdb works fine
        self.secretsdb = self._getCurrentFormatDb()
        self.assertEquals(None,
            update_secrets(self.referencedb, self.secretsdb, dummymessage))

    def test_update_modules(self):
        empty_db = self._getEmptyDb()
        update_secrets(self.referencedb, empty_db, dummymessage)
        newmodules = empty_db.search(
            expression="dn=@MODULES", base="", scope=SCOPE_SUBTREE)
        refmodules = self.referencedb.search(
            expression="dn=@MODULES", base="", scope=SCOPE_SUBTREE)
        self.assertEquals(newmodules.msgs, refmodules.msgs)

    def tearDown(self):
        for name in ["ref.ldb", "secrets.ldb"]:
            path = os.path.join(self.tempdir, name)
            if os.path.exists(path):
                os.unlink(path)
        super(UpdateSecretsTests, self).tearDown()


