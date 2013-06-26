#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
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

"""Tests for samba.samba3."""

from samba.samba3 import (GroupMappingDatabase, Registry, PolicyDatabase,
        SecretsDatabase, TdbSam)
from samba.samba3 import (WinsDatabase, SmbpasswdFile, ACB_NORMAL,
        IdmapDatabase, SAMUser, ParamFile)
from samba.tests import TestCase
import os

for p in [ "../../../../../testdata/samba3", "../../../../testdata/samba3" ]:
    DATADIR = os.path.join(os.path.dirname(__file__), p)
    if os.path.exists(DATADIR):
        break


class RegistryTestCase(TestCase):

    def setUp(self):
        super(RegistryTestCase, self).setUp()
        self.registry = Registry(os.path.join(DATADIR, "registry.tdb"))

    def tearDown(self):
        self.registry.close()
        super(RegistryTestCase, self).tearDown()

    def test_length(self):
        self.assertEquals(28, len(self.registry))

    def test_keys(self):
        self.assertTrue("HKLM" in self.registry.keys())

    def test_subkeys(self):
        self.assertEquals(["SOFTWARE", "SYSTEM"], self.registry.subkeys("HKLM"))

    def test_values(self):
        self.assertEquals({'DisplayName': (1L, 'E\x00v\x00e\x00n\x00t\x00 \x00L\x00o\x00g\x00\x00\x00'), 
                           'ErrorControl': (4L, '\x01\x00\x00\x00')}, 
                           self.registry.values("HKLM/SYSTEM/CURRENTCONTROLSET/SERVICES/EVENTLOG"))


class PolicyTestCase(TestCase):

    def setUp(self):
        super(PolicyTestCase, self).setUp()
        self.policy = PolicyDatabase(os.path.join(DATADIR, "account_policy.tdb"))

    def test_policy(self):
        self.assertEquals(self.policy.min_password_length, 5)
        self.assertEquals(self.policy.minimum_password_age, 0)
        self.assertEquals(self.policy.maximum_password_age, 999999999)
        self.assertEquals(self.policy.refuse_machine_password_change, 0)
        self.assertEquals(self.policy.reset_count_minutes, 0)
        self.assertEquals(self.policy.disconnect_time, -1)
        self.assertEquals(self.policy.user_must_logon_to_change_password, None)
        self.assertEquals(self.policy.password_history, 0)
        self.assertEquals(self.policy.lockout_duration, 0)
        self.assertEquals(self.policy.bad_lockout_minutes, None)


class GroupsTestCase(TestCase):

    def setUp(self):
        super(GroupsTestCase, self).setUp()
        self.groupdb = GroupMappingDatabase(os.path.join(DATADIR, "group_mapping.tdb"))

    def tearDown(self):
        self.groupdb.close()
        super(GroupsTestCase, self).tearDown()

    def test_group_length(self):
        self.assertEquals(13, len(list(self.groupdb.groupsids())))

    def test_get_group(self):
        self.assertEquals((-1, 5L, 'Administrators', ''), self.groupdb.get_group("S-1-5-32-544"))

    def test_groupsids(self):
        sids = list(self.groupdb.groupsids())
        self.assertTrue("S-1-5-32-544" in sids)

    def test_alias_length(self):
        self.assertEquals(0, len(list(self.groupdb.aliases())))


class SecretsDbTestCase(TestCase):

    def setUp(self):
        super(SecretsDbTestCase, self).setUp()
        self.secretsdb = SecretsDatabase(os.path.join(DATADIR, "secrets.tdb"))

    def tearDown(self):
        self.secretsdb.close()
        super(SecretsDbTestCase, self).tearDown()

    def test_get_sid(self):
        self.assertTrue(self.secretsdb.get_sid("BEDWYR") is not None)


class TdbSamTestCase(TestCase):

    def setUp(self):
        super(TdbSamTestCase, self).setUp()
        self.samdb = TdbSam(os.path.join(DATADIR, "passdb.tdb"))

    def tearDown(self):
        self.samdb.close()
        super(TdbSamTestCase, self).tearDown()

    def test_usernames(self):
        self.assertEquals(3, len(list(self.samdb.usernames())))

    def test_getuser(self):
        user = SAMUser("root")
        user.logoff_time = 2147483647
        user.kickoff_time = 2147483647
        user.pass_can_change_time = 1125418267
        user.username = "root"
        user.uid = None
        user.lm_password = 'U)\x02\x03\x1b\xed\xe9\xef\xaa\xd3\xb45\xb5\x14\x04\xee'
        user.nt_password = '\x87\x8d\x80\x14`l\xda)gzD\xef\xa15?\xc7'
        user.acct_ctrl = 16
        user.pass_last_set_time = 1125418267
        user.fullname = "root"
        user.nt_username = ""
        user.logoff_time = 2147483647
        user.acct_desc = ""
        user.group_rid = 1001
        user.logon_count = 0
        user.bad_password_count = 0
        user.domain = "BEDWYR"
        user.munged_dial = ""
        user.workstations = ""
        user.user_rid = 1000
        user.kickoff_time = 2147483647
        user.logoff_time = 2147483647
        user.unknown_6 = 1260L
        user.logon_divs = 0
        user.hours = [True for i in range(168)]
        other = self.samdb["root"]
        for name in other.__dict__:
            if other.__dict__[name] != user.__dict__[name]:
                print "%s: %r != %r" % (name, other.__dict__[name], user.__dict__[name])
        self.assertEquals(user, other)


class WinsDatabaseTestCase(TestCase):

    def setUp(self):
        super(WinsDatabaseTestCase, self).setUp()
        self.winsdb = WinsDatabase(os.path.join(DATADIR, "wins.dat"))

    def test_length(self):
        self.assertEquals(22, len(self.winsdb))

    def test_first_entry(self):
        self.assertEqual((1124185120, ["192.168.1.5"], 0x64), self.winsdb["ADMINISTRATOR#03"])

    def tearDown(self):
        self.winsdb.close()
        super(WinsDatabaseTestCase, self).tearDown()


class SmbpasswdTestCase(TestCase):

    def setUp(self):
        super(SmbpasswdTestCase, self).setUp()
        self.samdb = SmbpasswdFile(os.path.join(DATADIR, "smbpasswd"))

    def test_length(self):
        self.assertEquals(3, len(self.samdb))

    def test_get_user(self):
        user = SAMUser("rootpw")
        user.lm_password = "552902031BEDE9EFAAD3B435B51404EE"
        user.nt_password = "878D8014606CDA29677A44EFA1353FC7"
        user.acct_ctrl = ACB_NORMAL
        user.pass_last_set_time = int(1125418267)
        user.uid = 0
        self.assertEquals(user, self.samdb["rootpw"])

    def tearDown(self):
        self.samdb.close()
        super(SmbpasswdTestCase, self).tearDown()


class IdmapDbTestCase(TestCase):

    def setUp(self):
        super(IdmapDbTestCase, self).setUp()
        self.idmapdb = IdmapDatabase(os.path.join(DATADIR,
            "winbindd_idmap.tdb"))

    def test_user_hwm(self):
        self.assertEquals(10000, self.idmapdb.get_user_hwm())

    def test_group_hwm(self):
        self.assertEquals(10002, self.idmapdb.get_group_hwm())

    def test_uids(self):
        self.assertEquals(1, len(list(self.idmapdb.uids())))

    def test_gids(self):
        self.assertEquals(3, len(list(self.idmapdb.gids())))

    def test_get_user_sid(self):
        self.assertEquals("S-1-5-21-58189338-3053988021-627566699-501", self.idmapdb.get_user_sid(65534))

    def test_get_group_sid(self):
        self.assertEquals("S-1-5-21-2447931902-1787058256-3961074038-3007", self.idmapdb.get_group_sid(10001))

    def tearDown(self):
        self.idmapdb.close()
        super(IdmapDbTestCase, self).tearDown()


class ParamTestCase(TestCase):

    def test_init(self):
        file = ParamFile()
        self.assertTrue(file is not None)

    def test_add_section(self):
        file = ParamFile()
        file.add_section("global")
        self.assertTrue(file["global"] is not None)

    def test_set_param_string(self):
        file = ParamFile()
        file.add_section("global")
        file.set_string("data", "bar")
        self.assertEquals("bar", file.get_string("data"))

    def test_get_section(self):
        file = ParamFile()
        self.assertEquals(None, file.get_section("unknown"))
        self.assertRaises(KeyError, lambda: file["unknown"])
