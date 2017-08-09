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

"""Tests for samba.dcerpc.bare."""

from samba.dcerpc import ClientConnection
import samba.tests

class BareTestCase(samba.tests.TestCase):

    def test_bare(self):
        # Connect to the echo pipe
        x = ClientConnection("ncalrpc:localhost[DEFAULT]", 
                ("60a15ec5-4de8-11d7-a637-005056a20182", 1),
                lp_ctx=samba.tests.env_loadparm())
        self.assertEquals("\x01\x00\x00\x00", x.request(0, chr(0) * 4))

    def test_alter_context(self):
        x = ClientConnection("ncalrpc:localhost[DEFAULT]", 
                ("12345778-1234-abcd-ef00-0123456789ac", 1),
                lp_ctx=samba.tests.env_loadparm())
        y = ClientConnection("ncalrpc:localhost", 
                ("60a15ec5-4de8-11d7-a637-005056a20182", 1),
                basis_connection=x, lp_ctx=samba.tests.env_loadparm())
        x.alter_context(("60a15ec5-4de8-11d7-a637-005056a20182", 1))
        # FIXME: self.assertEquals("\x01\x00\x00\x00", x.request(0, chr(0) * 4))

    def test_two_connections(self):
        x = ClientConnection("ncalrpc:localhost[DEFAULT]", 
                ("60a15ec5-4de8-11d7-a637-005056a20182", 1), 
                lp_ctx=samba.tests.env_loadparm())
        y = ClientConnection("ncalrpc:localhost", 
                ("60a15ec5-4de8-11d7-a637-005056a20182", 1),
                basis_connection=x, lp_ctx=samba.tests.env_loadparm())
        self.assertEquals("\x01\x00\x00\x00", y.request(0, chr(0) * 4))
