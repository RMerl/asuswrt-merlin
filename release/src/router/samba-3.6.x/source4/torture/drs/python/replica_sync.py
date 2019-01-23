#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Tests various schema replication scenarios
#
# Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2011
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
#  export DC1=dc1_dns_name
#  export DC2=dc2_dns_name
#  export SUBUNITRUN=$samba4srcdir/scripting/bin/subunitrun
#  PYTHONPATH="$PYTHONPATH:$samba4srcdir/torture/drs/python" $SUBUNITRUN replica_sync -U"$DOMAIN/$DC_USERNAME"%"$DC_PASSWORD"
#

import drs_base
import samba.tests


class DrsReplicaSyncTestCase(drs_base.DrsBaseTestCase):
    """Intended as a black box test case for DsReplicaSync
       implementation. It should test the behavior of this
       case in cases when inbound replication is disabled"""

    def setUp(self):
        super(DrsReplicaSyncTestCase, self).setUp()

    def tearDown(self):
        # re-enable replication
        self._enable_inbound_repl(self.dnsname_dc1)
        super(DrsReplicaSyncTestCase, self).tearDown()

    def test_ReplEnabled(self):
        """Tests we can replicate when replication is enabled"""
        self._enable_inbound_repl(self.dnsname_dc1)
        self._net_drs_replicate(DC=self.dnsname_dc1, fromDC=self.dnsname_dc2, forced=False)

    def test_ReplDisabled(self):
        """Tests we cann't replicate when replication is disabled"""
        self._disable_inbound_repl(self.dnsname_dc1)
        try:
            self._net_drs_replicate(DC=self.dnsname_dc1, fromDC=self.dnsname_dc2, forced=False)
        except samba.tests.BlackboxProcessError, e:
            self.assertTrue('WERR_DS_DRA_SINK_DISABLED' in e.stderr)
        else:
            self.fail("'drs replicate' command should have failed!")

    def test_ReplDisabledForced(self):
        """Tests we cann't replicate when replication is disabled"""
        self._disable_inbound_repl(self.dnsname_dc1)
        out = self._net_drs_replicate(DC=self.dnsname_dc1, fromDC=self.dnsname_dc2, forced=True)
