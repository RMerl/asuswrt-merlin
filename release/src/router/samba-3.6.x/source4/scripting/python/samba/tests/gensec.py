#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2009
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

"""Tests for GENSEC.

Note that this just tests the bindings work. It does not intend to test 
the functionality, that's already done in other tests.
"""

from samba.credentials import Credentials
from samba import gensec
import samba.tests

class GensecTests(samba.tests.TestCase):

    def setUp(self):
        super(GensecTests, self).setUp()
        self.settings = {}
        self.settings["lp_ctx"] = self.lp_ctx = samba.tests.env_loadparm()
        self.settings["target_hostname"] = self.lp_ctx.get("netbios name")
        """This is just for the API tests"""
        self.gensec = gensec.Security.start_client(self.settings)

    def test_start_mech_by_unknown_name(self):
        self.assertRaises(RuntimeError, self.gensec.start_mech_by_name, "foo")

    def test_start_mech_by_name(self):
        self.gensec.start_mech_by_name("spnego")

    def test_info_uninitialized(self):
        self.assertRaises(RuntimeError, self.gensec.session_info)

    def test_update(self):
        """Test GENSEC by doing an exchange with ourselves using GSSAPI against a KDC"""

        """Start up a client and server GENSEC instance to test things with"""

        self.gensec_client = gensec.Security.start_client(self.settings)
        self.gensec_client.set_credentials(self.get_credentials())
        self.gensec_client.want_feature(gensec.FEATURE_SEAL)
        self.gensec_client.start_mech_by_sasl_name("GSSAPI")

        self.gensec_server = gensec.Security.start_server(self.settings)
        creds = Credentials()
        creds.guess(self.lp_ctx)
        creds.set_machine_account(self.lp_ctx)
        self.gensec_server.set_credentials(creds)

        self.gensec_server.want_feature(gensec.FEATURE_SEAL)
        self.gensec_server.start_mech_by_sasl_name("GSSAPI")

        client_finished = False
        server_finished = False
        server_to_client = ""
        
        """Run the actual call loop"""
        while client_finished == False and server_finished == False:
            if not client_finished:
                print "running client gensec_update"
                (client_finished, client_to_server) = self.gensec_client.update(server_to_client)
            if not server_finished:
                print "running server gensec_update"
                (server_finished, server_to_client) = self.gensec_server.update(client_to_server)
        session_info = self.gensec_server.session_info()

        test_string = "Hello Server"
        test_wrapped = self.gensec_client.wrap(test_string)
        test_unwrapped = self.gensec_server.unwrap(test_wrapped)
        self.assertEqual(test_string, test_unwrapped)
        test_string = "Hello Client"
        test_wrapped = self.gensec_server.wrap(test_string)
        test_unwrapped = self.gensec_client.unwrap(test_wrapped)
        self.assertEqual(test_string, test_unwrapped)
        
