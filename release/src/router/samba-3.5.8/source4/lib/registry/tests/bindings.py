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

import os
import unittest
from samba import registry
import samba.tests

class HelperTests(unittest.TestCase):
    def test_predef_to_name(self):
        self.assertEquals("HKEY_LOCAL_MACHINE", 
                          registry.get_predef_name(0x80000002))

    def test_str_regtype(self):
        self.assertEquals("REG_DWORD", registry.str_regtype(4))



class HiveTests(samba.tests.TestCaseInTempDir):
    def setUp(self):
        super(HiveTests, self).setUp()
        self.hive_path = os.path.join(self.tempdir, "ldb_new.ldb")
        self.hive = registry.open_ldb(self.hive_path)

    def tearDown(self):
        del self.hive
        os.unlink(self.hive_path)

    def test_ldb_new(self):
        self.assertTrue(self.hive is not None)

    #def test_flush(self):
    #    self.hive.flush()

    #def test_del_value(self):
    #    self.hive.del_value("FOO")


class RegistryTests(unittest.TestCase):
    def test_new(self):
        self.registry = registry.Registry()
