#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Unix SMB/CIFS implementation.
# Copyright © Jelmer Vernooij <jelmer@samba.org> 2008
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

"""Tests for samba.dcerpc.sam."""

from samba.dcerpc import samr, security
from samba.tests import RpcInterfaceTestCase

# FIXME: Pidl should be doing this for us
def toArray((handle, array, num_entries)):
    ret = []
    for x in range(num_entries):
        ret.append((array.entries[x].idx, array.entries[x].name))
    return ret


class SamrTests(RpcInterfaceTestCase):

    def setUp(self):
        super(SamrTests, self).setUp()
        self.conn = samr.samr("ncalrpc:", self.get_loadparm())

    def test_connect5(self):
        (level, info, handle) = self.conn.Connect5(None, 0, 1, samr.ConnectInfo1())

    def test_connect2(self):
        handle = self.conn.Connect2(None, security.SEC_FLAG_MAXIMUM_ALLOWED)
        self.assertTrue(handle is not None)

    def test_EnumDomains(self):
        handle = self.conn.Connect2(None, security.SEC_FLAG_MAXIMUM_ALLOWED)
        domains = toArray(self.conn.EnumDomains(handle, 0, -1))
        self.conn.Close(handle)

