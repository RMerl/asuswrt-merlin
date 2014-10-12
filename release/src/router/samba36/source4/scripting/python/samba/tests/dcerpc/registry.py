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

"""Tests for samba.dcerpc.registry."""

from samba.dcerpc import winreg
from samba.tests import RpcInterfaceTestCase


class WinregTests(RpcInterfaceTestCase):

    def setUp(self):
        super(WinregTests, self).setUp()
        self.conn = winreg.winreg("ncalrpc:", self.get_loadparm(), 
                                  self.get_credentials())

    def get_hklm(self):
        return self.conn.OpenHKLM(None, 
             winreg.KEY_QUERY_VALUE | winreg.KEY_ENUMERATE_SUB_KEYS)

    def test_hklm(self):
        handle = self.conn.OpenHKLM(None, 
                 winreg.KEY_QUERY_VALUE | winreg.KEY_ENUMERATE_SUB_KEYS)
        self.conn.CloseKey(handle)

    def test_getversion(self):
        handle = self.get_hklm()
        version = self.conn.GetVersion(handle)
        self.assertEquals(int, version.__class__)
        self.conn.CloseKey(handle)

    def test_getkeyinfo(self):
        handle = self.conn.OpenHKLM(None, 
                 winreg.KEY_QUERY_VALUE | winreg.KEY_ENUMERATE_SUB_KEYS)
        x = self.conn.QueryInfoKey(handle, winreg.String())
        self.assertEquals(9, len(x)) # should return a 9-tuple
        self.conn.CloseKey(handle)
