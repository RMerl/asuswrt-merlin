#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
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

"""Tests for samba.dcerpc.unixinfo."""


from samba.dcerpc import unixinfo
from samba.tests import RpcInterfaceTestCase

class UnixinfoTests(RpcInterfaceTestCase):

    def setUp(self):
        super(UnixinfoTests, self).setUp()
        self.conn = unixinfo.unixinfo("ncalrpc:", self.get_loadparm())

    def test_getpwuid_int(self):
        infos = self.conn.GetPWUid(range(512))
        self.assertEquals(512, len(infos))
        self.assertEquals("/bin/false", infos[0].shell)
        self.assertTrue(isinstance(infos[0].homedir, unicode))

    def test_getpwuid(self):
        infos = self.conn.GetPWUid(map(long, range(512)))
        self.assertEquals(512, len(infos))
        self.assertEquals("/bin/false", infos[0].shell)
        self.assertTrue(isinstance(infos[0].homedir, unicode))

    def test_gidtosid(self):
        self.conn.GidToSid(1000L)

    def test_uidtosid(self):
        self.conn.UidToSid(1000)
    
    def test_uidtosid_fail(self):
        self.assertRaises(TypeError, self.conn.UidToSid, "100")
