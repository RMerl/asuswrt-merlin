#!/usr/bin/env python

# Unix SMB/CIFS implementation. Tests for dsdb
# Copyright (C) Matthieu Patou <mat@matws.net> 2010
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

"""Tests for samba.dsdb."""

from samba.credentials import Credentials
from samba.samdb import SamDB
from samba.auth import system_session
from testtools.testcase import TestCase
from samba.ndr import ndr_unpack, ndr_pack
from samba.dcerpc import drsblobs
import ldb
import os
import samba


class DsdbTests(TestCase):


    def setUp(self):
        super(DsdbTests, self).setUp()
        self.lp = samba.param.LoadParm()
        self.lp.load(os.path.join(os.path.join(self.baseprovpath(), "etc"), "smb.conf"))
        self.creds = Credentials()
        self.creds.guess(self.lp)
        self.session = system_session()
        self.samdb = SamDB(os.path.join(self.baseprovpath(), "private", "sam.ldb"),
            session_info=self.session, credentials=self.creds,lp=self.lp)


    def baseprovpath(self):
        return os.path.join(os.environ['SELFTEST_PREFIX'], "dc")


    def test_get_oid_from_attrid(self):
        oid = self.samdb.get_oid_from_attid(591614)
        self.assertEquals(oid, "1.2.840.113556.1.4.1790")

    def test_error_replpropertymetadata(self):
        res = self.samdb.search(expression="cn=Administrator",
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["replPropertyMetaData"])
        repl = ndr_unpack(drsblobs.replPropertyMetaDataBlob,
                            str(res[0]["replPropertyMetaData"]))
        ctr = repl.ctr
        for o in ctr.array:
            # Search for Description
            if o.attid == 13:
                old_version = o.version
                o.version = o.version + 1
        replBlob = ndr_pack(repl)
        msg = ldb.Message()
        msg.dn = res[0].dn
        msg["replPropertyMetaData"] = ldb.MessageElement(replBlob, ldb.FLAG_MOD_REPLACE, "replPropertyMetaData")
        self.assertRaises(ldb.LdbError, self.samdb.modify, msg, ["local_oid:1.3.6.1.4.1.7165.4.3.14:0"])

    def test_twoatt_replpropertymetadata(self):
        res = self.samdb.search(expression="cn=Administrator",
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["replPropertyMetaData", "uSNChanged"])
        repl = ndr_unpack(drsblobs.replPropertyMetaDataBlob,
                            str(res[0]["replPropertyMetaData"]))
        ctr = repl.ctr
        for o in ctr.array:
            # Search for Description
            if o.attid == 13:
                old_version = o.version
                o.version = o.version + 1
                o.local_usn = long(str(res[0]["uSNChanged"])) + 1
        replBlob = ndr_pack(repl)
        msg = ldb.Message()
        msg.dn = res[0].dn
        msg["replPropertyMetaData"] = ldb.MessageElement(replBlob, ldb.FLAG_MOD_REPLACE, "replPropertyMetaData")
        msg["description"] = ldb.MessageElement("new val", ldb.FLAG_MOD_REPLACE, "description")
        self.assertRaises(ldb.LdbError, self.samdb.modify, msg, ["local_oid:1.3.6.1.4.1.7165.4.3.14:0"])

    def test_set_replpropertymetadata(self):
        res = self.samdb.search(expression="cn=Administrator",
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["replPropertyMetaData", "uSNChanged"])
        repl = ndr_unpack(drsblobs.replPropertyMetaDataBlob,
                            str(res[0]["replPropertyMetaData"]))
        ctr = repl.ctr
        for o in ctr.array:
            # Search for Description
            if o.attid == 13:
                old_version = o.version
                o.version = o.version + 1
                o.local_usn = long(str(res[0]["uSNChanged"])) + 1
                o.originating_usn = long(str(res[0]["uSNChanged"])) + 1
        replBlob = ndr_pack(repl)
        msg = ldb.Message()
        msg.dn = res[0].dn
        msg["replPropertyMetaData"] = ldb.MessageElement(replBlob, ldb.FLAG_MOD_REPLACE, "replPropertyMetaData")
        self.samdb.modify(msg, ["local_oid:1.3.6.1.4.1.7165.4.3.14:0"])

    def test_ok_get_attribute_from_attid(self):
        self.assertEquals(self.samdb.get_attribute_from_attid(13), "description")

    def test_ko_get_attribute_from_attid(self):
        self.assertEquals(self.samdb.get_attribute_from_attid(11979), None)

    def test_get_attribute_replmetadata_version(self):
        res = self.samdb.search(expression="cn=Administrator",
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["dn"])
        self.assertEquals(len(res), 1)
        dn = str(res[0].dn)
        self.assertEqual(self.samdb.get_attribute_replmetadata_version(dn, "unicodePwd"), 1)

    def test_set_attribute_replmetadata_version(self):
        res = self.samdb.search(expression="cn=Administrator",
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["dn"])
        self.assertEquals(len(res), 1)
        dn = str(res[0].dn)
        version = self.samdb.get_attribute_replmetadata_version(dn, "description")
        self.samdb.set_attribute_replmetadata_version(dn, "description", version + 2)
        self.assertEqual(self.samdb.get_attribute_replmetadata_version(dn, "description"), version + 2)
