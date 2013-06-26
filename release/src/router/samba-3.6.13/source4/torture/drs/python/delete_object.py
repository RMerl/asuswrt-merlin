#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Unix SMB/CIFS implementation.
# Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010
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

#
# Usage:
#  export DC1=dc1_dns_name
#  export DC2=dc2_dns_name
#  export SUBUNITRUN=$samba4srcdir/scripting/bin/subunitrun
#  PYTHONPATH="$PYTHONPATH:$samba4srcdir/torture/drs/python" $SUBUNITRUN delete_object -U"$DOMAIN/$DC_USERNAME"%"$DC_PASSWORD"
#

import time


from ldb import (
    SCOPE_SUBTREE
    )

import drs_base


class DrsDeleteObjectTestCase(drs_base.DrsBaseTestCase):

    def setUp(self):
        super(DrsDeleteObjectTestCase, self).setUp()
        # make sure DCs are synchronized before the test
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, forced=True)
        self._net_drs_replicate(DC=self.dnsname_dc1, fromDC=self.dnsname_dc2, forced=True)
        # disable automatic replication temporary
        self._disable_inbound_repl(self.dnsname_dc1)
        self._disable_inbound_repl(self.dnsname_dc2)

    def tearDown(self):
        self._enable_inbound_repl(self.dnsname_dc1)
        self._enable_inbound_repl(self.dnsname_dc2)
        super(DrsDeleteObjectTestCase, self).tearDown()

    def _make_username(self):
        return "DrsDelObjUser_" + time.strftime("%s", time.gmtime())

    def _check_user(self, sam_ldb, user_orig, is_deleted):
        # search the user by guid as it may be deleted
        guid_str = self._GUID_string(user_orig["objectGUID"][0])
        expression = "(objectGUID=%s)" % guid_str
        res = sam_ldb.search(base=self.domain_dn,
                             expression=expression,
                             controls=["show_deleted:1"])
        self.assertEquals(len(res), 1)
        user_cur = res[0]
        # Deleted Object base DN
        dodn = self._deleted_objects_dn(sam_ldb)
        # now check properties of the user
        name_orig = user_orig["cn"][0]
        name_cur  = user_cur["cn"][0]
        if is_deleted:
            self.assertEquals(user_cur["isDeleted"][0],"TRUE")
            self.assertTrue(not("objectCategory" in user_cur))
            self.assertTrue(not("sAMAccountType" in user_cur))
            self.assertTrue(dodn in str(user_cur["dn"]),
                            "User %s is deleted but it is not located under %s!" % (name_orig, dodn))
            self.assertEquals(name_cur, name_orig + "\nDEL:" + guid_str)
        else:
            self.assertTrue(not("isDeleted" in user_cur))
            self.assertEquals(name_cur, name_orig)
            self.assertEquals(user_orig["dn"], user_cur["dn"])
            self.assertTrue(dodn not in str(user_cur["dn"]))

    def test_ReplicateDeteleteObject(self):
        """Verifies how a deleted-object is replicated between two DCs.
           This test should verify that:
            - deleted-object is replicated properly
           TODO: We should verify that after replication,
                 object's state to conform to a deleted-object state
                 or tombstone -object, depending on DC's features
                 It will also be great if check replPropertyMetaData."""
        # work-out unique username to test with
        username = self._make_username()

        # create user on DC1
        self.ldb_dc1.newuser(username=username, password="P@sswOrd!")
        ldb_res = self.ldb_dc1.search(base=self.domain_dn,
                                      scope=SCOPE_SUBTREE,
                                      expression="(samAccountName=%s)" % username)
        self.assertEquals(len(ldb_res), 1)
        user_orig = ldb_res[0]
        user_dn   = ldb_res[0]["dn"]

        # check user info on DC1
        print "Testing for %s with GUID %s" % (username, self._GUID_string(user_orig["objectGUID"][0]))
        self._check_user(sam_ldb=self.ldb_dc1, user_orig=user_orig, is_deleted=False)

        # trigger replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, forced=True)

        # delete user on DC1
        self.ldb_dc1.delete(user_dn)
        # check user info on DC1 - should be deleted
        self._check_user(sam_ldb=self.ldb_dc1, user_orig=user_orig, is_deleted=True)
        # check user info on DC2 - should be valid user
        self._check_user(sam_ldb=self.ldb_dc2, user_orig=user_orig, is_deleted=False)

        # trigger replication from DC2 to DC1
        # to check if deleted object gets restored
        self._net_drs_replicate(DC=self.dnsname_dc1, fromDC=self.dnsname_dc2, forced=True)
        # check user info on DC1 - should be deleted
        self._check_user(sam_ldb=self.ldb_dc1, user_orig=user_orig, is_deleted=True)
        # check user info on DC2 - should be valid user
        self._check_user(sam_ldb=self.ldb_dc2, user_orig=user_orig, is_deleted=False)

        # trigger replication from DC1 to DC2
        # to check if deleted object is replicated
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, forced=True)
        # check user info on DC1 - should be deleted
        self._check_user(sam_ldb=self.ldb_dc1, user_orig=user_orig, is_deleted=True)
        # check user info on DC2 - should be deleted
        self._check_user(sam_ldb=self.ldb_dc2, user_orig=user_orig, is_deleted=True)

