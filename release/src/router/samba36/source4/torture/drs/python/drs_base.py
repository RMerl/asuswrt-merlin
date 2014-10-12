#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Unix SMB/CIFS implementation.
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


import sys
import time
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

from ldb import (
    SCOPE_BASE,
    Message,
    FLAG_MOD_REPLACE,
    )

import samba.tests


class DrsBaseTestCase(samba.tests.BlackboxTestCase):
    """Base class implementation for all DRS python tests.
       It is intended to provide common initialization and
       and functionality used by all DRS tests in drs/python
       test package. For instance, DC1 and DC2 are always used
       to pass URLs for DCs to test against"""

    def setUp(self):
        super(DrsBaseTestCase, self).setUp()

        # connect to DCs
        url_dc = samba.tests.env_get_var_value("DC1")
        (self.ldb_dc1, self.info_dc1) = samba.tests.connect_samdb_ex(url_dc,
                                                                     ldap_only=True)
        url_dc = samba.tests.env_get_var_value("DC2")
        (self.ldb_dc2, self.info_dc2) = samba.tests.connect_samdb_ex(url_dc,
                                                                     ldap_only=True)

        # cache some of RootDSE props
        self.schema_dn = self.info_dc1["schemaNamingContext"][0]
        self.domain_dn = self.info_dc1["defaultNamingContext"][0]
        self.config_dn = self.info_dc1["configurationNamingContext"][0]
        self.forest_level = int(self.info_dc1["forestFunctionality"][0])

        # we will need DCs DNS names for 'samba-tool drs' command
        self.dnsname_dc1 = self.info_dc1["dnsHostName"][0]
        self.dnsname_dc2 = self.info_dc2["dnsHostName"][0]

    def tearDown(self):
        super(DrsBaseTestCase, self).tearDown()

    def _GUID_string(self, guid):
        return self.ldb_dc1.schema_format_value("objectGUID", guid)

    def _ldap_schemaUpdateNow(self, sam_db):
        rec = {"dn": "",
               "schemaUpdateNow": "1"}
        m = Message.from_dict(sam_db, rec, FLAG_MOD_REPLACE)
        sam_db.modify(m)

    def _deleted_objects_dn(self, sam_ldb):
        wkdn = "<WKGUID=18E2EA80684F11D2B9AA00C04F79F805,%s>" % self.domain_dn
        res = sam_ldb.search(base=wkdn,
                             scope=SCOPE_BASE,
                             controls=["show_deleted:1"])
        self.assertEquals(len(res), 1)
        return str(res[0]["dn"])

    def _make_obj_name(self, prefix):
        return prefix + time.strftime("%s", time.gmtime())

    def _samba_tool_cmdline(self, drs_command):
        # find out where is net command
        samba_tool_cmd = os.path.abspath("./bin/samba-tool")
        # make command line credentials string
        creds = self.get_credentials()
        cmdline_auth = "-U%s/%s%%%s" % (creds.get_domain(),
                                        creds.get_username(), creds.get_password())
        # bin/samba-tool drs <drs_command> <cmdline_auth>
        return "%s drs %s %s" % (samba_tool_cmd, drs_command, cmdline_auth)

    def _net_drs_replicate(self, DC, fromDC, nc_dn=None, forced=True):
        if nc_dn is None:
            nc_dn = self.domain_dn
        # make base command line
        samba_tool_cmdline = self._samba_tool_cmdline("replicate")
        if forced:
            samba_tool_cmdline += " --sync-forced"
        # bin/samba-tool drs replicate <Dest_DC_NAME> <Src_DC_NAME> <Naming Context>
        cmd_line = "%s %s %s %s" % (samba_tool_cmdline, DC, fromDC, nc_dn)
        return self.check_output(cmd_line)

    def _enable_inbound_repl(self, DC):
        # make base command line
        samba_tool_cmd = self._samba_tool_cmdline("options")
        # disable replication
        self.check_run("%s %s --dsa-option=-DISABLE_INBOUND_REPL" %(samba_tool_cmd, DC))

    def _disable_inbound_repl(self, DC):
        # make base command line
        samba_tool_cmd = self._samba_tool_cmdline("options")
        # disable replication
        self.check_run("%s %s --dsa-option=+DISABLE_INBOUND_REPL" %(samba_tool_cmd, DC))
