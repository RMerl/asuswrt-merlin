#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Unix SMB/CIFS implementation.
# Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2009
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

#
# Usage:
#  export ACCOUNT_NAME=kamen
#  export NEW_PASS=test
#  export SUBUNITRUN=$samba4srcdir/scripting/bin/subunitrun
#  PYTHONPATH="$samba4srcdir/torture/libnet/python" $SUBUNITRUN samr-test -Ukma-exch.devel/Administrator%333
#

import os

from samba import net
import samba.tests

if not "ACCOUNT_NAME" in os.environ.keys():
    raise Exception("Please supply ACCOUNT_NAME in environment")

if not "NEW_PASS" in os.environ.keys():
    raise Exception("Please supply NEW_PASS in environment")

account_name = os.environ["ACCOUNT_NAME"]
new_pass = os.environ["NEW_PASS"]

#
# Tests start here
#

class Libnet_SetPwdTest(samba.tests.TestCase):

    ########################################################################################

    def test_SetPassword(self):
        creds = self.get_credentials()
        net.SetPassword(account_name=account_name,
                        domain_name=creds.get_domain(),
                        newpassword=new_pass,
                        credentials=creds)

    ########################################################################################

