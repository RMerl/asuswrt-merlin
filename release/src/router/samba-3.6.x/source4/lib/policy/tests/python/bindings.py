#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2010
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

"""Tests for the libpolicy Python bindings.

"""

import unittest
from samba import policy

class PolicyTests(unittest.TestCase):

    def test_get_gpo_flags(self):
        self.assertEquals(["GPO_FLAG_USER_DISABLE"],
            policy.get_gpo_flags(policy.GPO_FLAG_USER_DISABLE))

    def test_get_gplink_options(self):
        self.assertEquals(["GPLINK_OPT_DISABLE"],
            policy.get_gplink_options(policy.GPLINK_OPT_DISABLE))
