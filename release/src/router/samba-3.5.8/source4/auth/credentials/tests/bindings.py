#!/usr/bin/python

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

"""Tests for the Credentials Python bindings. 

Note that this just tests the bindings work. It does not intend to test 
the functionality, that's already done in other tests.
"""

import unittest
from samba import credentials

class CredentialsTests(unittest.TestCase):
    def setUp(self):
        self.creds = credentials.Credentials()

    def test_set_username(self):
        self.creds.set_username("somebody")
        self.assertEquals("somebody", self.creds.get_username())

    def test_set_password(self):
        self.creds.set_password("S3CreT")
        self.assertEquals("S3CreT", self.creds.get_password())

    def test_set_domain(self):
        self.creds.set_domain("ABMAS")
        self.assertEquals("ABMAS", self.creds.get_domain())

    def test_set_realm(self):
        self.creds.set_realm("myrealm")
        self.assertEquals("MYREALM", self.creds.get_realm())

    def test_parse_string_anon(self):
        self.creds.parse_string("%")
        self.assertEquals("", self.creds.get_username())
        self.assertEquals(None, self.creds.get_password())

    def test_parse_string_user_pw_domain(self):
        self.creds.parse_string("dom\\someone%secr")
        self.assertEquals("someone", self.creds.get_username())
        self.assertEquals("secr", self.creds.get_password())
        self.assertEquals("DOM", self.creds.get_domain())

    def test_bind_dn(self):
        self.assertEquals(None, self.creds.get_bind_dn())
        self.creds.set_bind_dn("dc=foo,cn=bar")
        self.assertEquals("dc=foo,cn=bar", self.creds.get_bind_dn())

    def test_is_anon(self):
        self.creds.set_username("")
        self.assertTrue(self.creds.is_anonymous())
        self.creds.set_username("somebody")
        self.assertFalse(self.creds.is_anonymous())
        self.creds.set_anonymous()
        self.assertTrue(self.creds.is_anonymous())

    def test_workstation(self):
        # FIXME: This is uninitialised, it should be None
        #self.assertEquals(None, self.creds.get_workstation())
        self.creds.set_workstation("myworksta")
        self.assertEquals("myworksta", self.creds.get_workstation())

    def test_get_nt_hash(self):
        self.creds.set_password("geheim")
        self.assertEquals('\xc2\xae\x1f\xe6\xe6H\x84cRE>\x81o*\xeb\x93', 
                          self.creds.get_nt_hash())

    def test_guess(self):
        # Just check the method is there and doesn't raise an exception
        self.creds.guess()

    def test_set_cmdline_callbacks(self):
        self.creds.set_cmdline_callbacks()

    def test_authentication_requested(self):
        self.creds.set_username("")
        self.assertFalse(self.creds.authentication_requested())
        self.creds.set_username("somebody")
        self.assertTrue(self.creds.authentication_requested())

    def test_wrong_password(self):
        self.assertFalse(self.creds.wrong_password())
