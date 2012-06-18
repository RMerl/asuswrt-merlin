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

from samba import param
import unittest

class LoadParmTestCase(unittest.TestCase):
    def test_init(self):
        file = param.LoadParm()
        self.assertTrue(file is not None)

    def test_length(self):
        file = param.LoadParm()
        self.assertEquals(0, len(file))

    def test_set_workgroup(self):
        file = param.LoadParm()
        file.set("workgroup", "bla")
        self.assertEquals("BLA", file.get("workgroup"))

    def test_is_mydomain(self):
        file = param.LoadParm()
        file.set("workgroup", "bla")
        self.assertTrue(file.is_mydomain("BLA"))
        self.assertFalse(file.is_mydomain("FOOBAR"))

    def test_is_myname(self):
        file = param.LoadParm()
        file.set("netbios name", "bla")
        self.assertTrue(file.is_myname("BLA"))
        self.assertFalse(file.is_myname("FOOBAR"))

    def test_load_default(self):
        file = param.LoadParm()
        file.load_default()

