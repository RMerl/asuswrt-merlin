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

from samba import Ldb
from samba.upgrade import import_wins
from samba.tests import LdbTestCase

class WinsUpgradeTests(LdbTestCase):
    def test_upgrade(self):
        winsdb = {
            "FOO#20": (200, ["127.0.0.1", "127.0.0.2"], 0x60)
        }
        import_wins(self.ldb, winsdb)

        self.assertEquals(['name=FOO,type=0x20'], 
                          [str(m.dn) for m in self.ldb.search(expression="(objectClass=winsRecord)")])

    def test_version(self):
        import_wins(self.ldb, {})
        self.assertEquals("VERSION", 
                str(self.ldb.search(expression="(objectClass=winsMaxVersion)")[0]["cn"]))
