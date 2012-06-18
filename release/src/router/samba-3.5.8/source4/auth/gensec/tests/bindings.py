#!/usr/bin/python

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

import unittest
from samba import gensec

class CredentialsTests(unittest.TestCase):

    def setUp(self):
        self.gensec = gensec.Security.start_client()

    def test_info(self):
        self.assertEquals(None, self.gensec.session_info())
