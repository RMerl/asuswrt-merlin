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

"""Tests for samba.dceprc.rpcecho."""

from samba.dcerpc import echo
from samba.ndr import ndr_pack, ndr_unpack
from samba.tests import RpcInterfaceTestCase, TestCase


class RpcEchoTests(RpcInterfaceTestCase):

    def setUp(self):
        super(RpcEchoTests, self).setUp()
        self.conn = echo.rpcecho("ncalrpc:", self.get_loadparm())

    def test_two_contexts(self):
        self.conn2 = echo.rpcecho("ncalrpc:", self.get_loadparm(), basis_connection=self.conn)
        self.assertEquals(3, self.conn2.AddOne(2))

    def test_abstract_syntax(self):
        self.assertEquals(("60a15ec5-4de8-11d7-a637-005056a20182", 1),
                          self.conn.abstract_syntax)

    def test_addone(self):
        self.assertEquals(2, self.conn.AddOne(1))

    def test_echodata(self):
        self.assertEquals([1,2,3], self.conn.EchoData([1, 2, 3]))

    def test_call(self):
        self.assertEquals(u"foobar", self.conn.TestCall(u"foobar"))

    def test_surrounding(self):
        surrounding_struct = echo.Surrounding()
        surrounding_struct.x = 4
        surrounding_struct.surrounding = [1,2,3,4]
        y = self.conn.TestSurrounding(surrounding_struct)
        self.assertEquals(8 * [0], y.surrounding)

    def test_manual_request(self):
        self.assertEquals("\x01\x00\x00\x00", self.conn.request(0, chr(0) * 4))

    def test_server_name(self):
        self.assertEquals(None, self.conn.server_name)


class NdrEchoTests(TestCase):

    def test_info1_push(self):
        x = echo.info1()
        x.v = 42
        self.assertEquals("\x2a", ndr_pack(x))

    def test_info1_pull(self):
        x = ndr_unpack(echo.info1, "\x42")
        self.assertEquals(x.v, 66)
