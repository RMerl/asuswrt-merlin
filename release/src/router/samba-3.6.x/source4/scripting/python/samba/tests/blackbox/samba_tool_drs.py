#!/usr/bin/env python

# Blackbox tests for "samba-tool drs" command
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

"""Blackbox tests for samba-tool drs."""

import os
import samba.tests


class SambaToolDrsTests(samba.tests.BlackboxTestCase):
    """Blackbox test case for samba-tool drs."""

    def setUp(self):
        super(SambaToolDrsTests, self).setUp()

        self.dc1 = samba.tests.env_get_var_value("DC1")
        self.dc2 = samba.tests.env_get_var_value("DC2")

        creds = self.get_credentials()
        self.cmdline_creds = "-U%s/%s%%%s" % (creds.get_domain(),
                                              creds.get_username(), creds.get_password())

    def _get_rootDSE(self, dc):
        samdb = samba.tests.connect_samdb(dc, lp=self.get_loadparm(),
                                          credentials=self.get_credentials(),
                                          ldap_only=True)
        return samdb.search(base="", scope=samba.tests.ldb.SCOPE_BASE)[0]

    def test_samba_tool_bind(self):
        """Tests 'samba-tool drs bind' command
           Output should be like:
               Extensions supported:
                 <list-of-supported-extensions>
               Site GUID: <GUID>
               Repl epoch: 0"""
        out = self.check_output("samba-tool drs bind %s %s" % (self.dc1,
                                                               self.cmdline_creds))
        self.assertTrue("Site GUID:" in out)
        self.assertTrue("Repl epoch:" in out)

    def test_samba_tool_kcc(self):
        """Tests 'samba-tool drs kcc' command
           Output should be like 'Consistency check on <DC> successful.'"""
        out = self.check_output("samba-tool drs kcc %s %s" % (self.dc1,
                                                              self.cmdline_creds))
        self.assertTrue("Consistency check on" in out)
        self.assertTrue("successful" in out)

    def test_samba_tool_showrepl(self):
        """Tests 'samba-tool drs showrepl' command
           Output should be like:
               <site-name>/<domain-name>
               DSA Options: <hex-options>
               DSA object GUID: <DSA-object-GUID>
               DSA invocationId: <DSA-invocationId>
               <Inbound-connections-list>
               <Outbound-connections-list>
               <KCC-objects>
               ...
            TODO: Perhaps we should check at least for
                  DSA's objectGUDI and invocationId"""
        out = self.check_output("samba-tool drs showrepl %s %s" % (self.dc1,
                                                                   self.cmdline_creds))
        self.assertTrue("DSA Options:" in out)
        self.assertTrue("DSA object GUID:" in out)
        self.assertTrue("DSA invocationId:" in out)

    def test_samba_tool_options(self):
        """Tests 'samba-tool drs options' command
           Output should be like 'Current DSA options: IS_GC <OTHER_FLAGS>'"""
        out = self.check_output("samba-tool drs options %s %s" % (self.dc1,
                                                                  self.cmdline_creds))
        self.assertTrue("Current DSA options:" in out)

    def test_samba_tool_replicate(self):
        """Tests 'samba-tool drs replicate' command
           Output should be like 'Replicate from <DC-SRC> to <DC-DEST> was successful.'"""
        nc_name = self._get_rootDSE(self.dc1)["defaultNamingContext"]
        out = self.check_output("samba-tool drs replicate %s %s %s %s" % (self.dc1,
                                                                          self.dc2,
                                                                          nc_name,
                                                                          self.cmdline_creds))
        self.assertTrue("Replicate from" in out)
        self.assertTrue("was successful" in out)
