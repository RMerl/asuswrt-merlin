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

"""Tests for samba.dcerpc.security."""

import samba.tests
from samba.dcerpc import security

class SecurityTokenTests(samba.tests.TestCase):

    def setUp(self):
        super(SecurityTokenTests, self).setUp()
        self.token = security.token()

    def test_is_system(self):
        self.assertFalse(self.token.is_system())

    def test_is_anonymous(self):
        self.assertFalse(self.token.is_anonymous())

    def test_has_builtin_administrators(self):
        self.assertFalse(self.token.has_builtin_administrators())

    def test_has_nt_authenticated_users(self):
        self.assertFalse(self.token.has_nt_authenticated_users())

    def test_has_priv(self):
        self.assertFalse(self.token.has_privilege(security.SEC_PRIV_SHUTDOWN))

    def test_set_priv(self):
        self.assertFalse(self.token.has_privilege(security.SEC_PRIV_SHUTDOWN))
        self.assertFalse(self.token.set_privilege(security.SEC_PRIV_SHUTDOWN))
        self.assertTrue(self.token.has_privilege(security.SEC_PRIV_SHUTDOWN))


class SecurityDescriptorTests(samba.tests.TestCase):

    def setUp(self):
        super(SecurityDescriptorTests, self).setUp()
        self.descriptor = security.descriptor()

    def test_from_sddl(self):
        desc = security.descriptor.from_sddl("O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)", security.dom_sid("S-2-0-0"))
        self.assertEquals(desc.group_sid, security.dom_sid('S-2-0-0-512'))
        self.assertEquals(desc.owner_sid, security.dom_sid('S-1-5-32-548'))
        self.assertEquals(desc.revision, 1)
        self.assertEquals(desc.sacl, None)
        self.assertEquals(desc.type, 0x8004)

    def test_from_sddl_invalidsddl(self):
        self.assertRaises(TypeError,security.descriptor.from_sddl, "foo",security.dom_sid("S-2-0-0"))

    def test_from_sddl_invalidtype1(self):
        self.assertRaises(TypeError, security.descriptor.from_sddl, security.dom_sid('S-2-0-0-512'),security.dom_sid("S-2-0-0"))

    def test_from_sddl_invalidtype2(self):
        sddl = "O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)"
        self.assertRaises(TypeError, security.descriptor.from_sddl, sddl,
                "S-2-0-0")

    def test_as_sddl(self):
        text = "O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)"
        dom = security.dom_sid("S-2-0-0")
        desc1 = security.descriptor.from_sddl(text, dom)
        desc2 = security.descriptor.from_sddl(desc1.as_sddl(dom), dom)
        self.assertEquals(desc1.group_sid, desc2.group_sid)
        self.assertEquals(desc1.owner_sid, desc2.owner_sid)
        self.assertEquals(desc1.sacl, desc2.sacl)
        self.assertEquals(desc1.type, desc2.type)

    def test_as_sddl_invalid(self):
        text = "O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)"
        dom = security.dom_sid("S-2-0-0")
        desc1 = security.descriptor.from_sddl(text, dom)
        self.assertRaises(TypeError, desc1.as_sddl,text)


    def test_as_sddl_no_domainsid(self):
        dom = security.dom_sid("S-2-0-0")
        text = "O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)"
        desc1 = security.descriptor.from_sddl(text, dom)
        desc2 = security.descriptor.from_sddl(desc1.as_sddl(), dom)
        self.assertEquals(desc1.group_sid, desc2.group_sid)
        self.assertEquals(desc1.owner_sid, desc2.owner_sid)
        self.assertEquals(desc1.sacl, desc2.sacl)
        self.assertEquals(desc1.type, desc2.type)

    def test_domsid_nodomsid_as_sddl(self):
        dom = security.dom_sid("S-2-0-0")
        text = "O:AOG:DAD:(A;;RPWPCCDCLCSWRCWDWOGA;;;S-1-0-0)"
        desc1 = security.descriptor.from_sddl(text, dom)
        self.assertNotEqual(desc1.as_sddl(), desc1.as_sddl(dom))

    def test_split(self):
        dom = security.dom_sid("S-2-0-7")
        self.assertEquals((security.dom_sid("S-2-0"), 7), dom.split())


class DomSidTests(samba.tests.TestCase):

    def test_parse_sid(self):
        sid = security.dom_sid("S-1-5-21")
        self.assertEquals("S-1-5-21", str(sid))

    def test_sid_equal(self):
        sid1 = security.dom_sid("S-1-5-21")
        sid2 = security.dom_sid("S-1-5-21")
        self.assertEquals(sid1, sid1)
        self.assertEquals(sid1, sid2)

    def test_random(self):
        sid = security.random_sid()
        self.assertTrue(str(sid).startswith("S-1-5-21-"))

    def test_repr(self):
        sid = security.random_sid()
        self.assertTrue(repr(sid).startswith("dom_sid('S-1-5-21-"))


class PrivilegeTests(samba.tests.TestCase):

    def test_privilege_name(self):
        self.assertEquals("SeShutdownPrivilege",
                security.privilege_name(security.SEC_PRIV_SHUTDOWN))

    def test_privilege_id(self):
        self.assertEquals(security.SEC_PRIV_SHUTDOWN,
                security.privilege_id("SeShutdownPrivilege"))

