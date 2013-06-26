#!/usr/bin/env python

# Unix SMB/CIFS implementation. Tests for SamDB
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
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

"""Tests for samba.samdb."""

import logging
import os
import uuid

from samba.auth import system_session
from samba.provision import (setup_samdb, guess_names, make_smbconf,
    provision_paths_from_lp)
from samba.provision import DEFAULT_POLICY_GUID, DEFAULT_DC_POLICY_GUID
from samba.provision.backend import ProvisionBackend
from samba.tests import TestCaseInTempDir
from samba.dcerpc import security
from samba.schema import Schema
from samba import param


class SamDBTestCase(TestCaseInTempDir):
    """Base-class for tests with a Sam Database.

    This is used by the Samba SamDB-tests, but e.g. also by the OpenChange
    provisioning tests (which need a Sam).
    """

    def setUp(self):
        super(SamDBTestCase, self).setUp()
        invocationid = str(uuid.uuid4())
        domaindn = "DC=COM,DC=EXAMPLE"
        self.domaindn = domaindn
        configdn = "CN=Configuration," + domaindn
        schemadn = "CN=Schema," + configdn
        domainguid = str(uuid.uuid4())
        policyguid = DEFAULT_POLICY_GUID
        domainsid = security.random_sid()
        path = os.path.join(self.tempdir, "samdb.ldb")
        session_info = system_session()
        
        hostname="foo"
        domain="EXAMPLE"
        dnsdomain="example.com" 
        serverrole="domain controller"
        policyguid_dc = DEFAULT_DC_POLICY_GUID

        smbconf = os.path.join(self.tempdir, "smb.conf")
        make_smbconf(smbconf, hostname, domain, dnsdomain,
                     serverrole, self.tempdir)

        self.lp = param.LoadParm()
        self.lp.load(smbconf)

        names = guess_names(lp=self.lp, hostname=hostname, 
                            domain=domain, dnsdomain=dnsdomain, 
                            serverrole=serverrole, 
                            domaindn=self.domaindn, configdn=configdn, 
                            schemadn=schemadn)

        paths = provision_paths_from_lp(self.lp, names.dnsdomain)

        logger = logging.getLogger("provision")

        provision_backend = ProvisionBackend("ldb", paths=paths,
                lp=self.lp, credentials=None,
                names=names, logger=logger)

        schema = Schema(domainsid, invocationid=invocationid,
                schemadn=names.schemadn, serverdn=names.serverdn,
                am_rodc=False)

        self.samdb = setup_samdb(path, session_info,
                provision_backend, self.lp, names, logger,
                domainsid, domainguid, policyguid, policyguid_dc, False,
                "secret", "secret", "secret", invocationid, "secret",
                None, "domain controller", schema=schema)

    def tearDown(self):
        for f in ['schema.ldb', 'configuration.ldb',
                  'users.ldb', 'samdb.ldb', 'smb.conf']:
            os.remove(os.path.join(self.tempdir, f))
        super(SamDBTestCase, self).tearDown()
