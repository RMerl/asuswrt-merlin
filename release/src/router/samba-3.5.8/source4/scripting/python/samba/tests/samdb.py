#!/usr/bin/python

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
from samba.auth import system_session
from samba.credentials import Credentials
import os
from samba.provision import setup_samdb, guess_names, setup_templatesdb, make_smbconf, find_setup_dir
from samba.samdb import SamDB
from samba.tests import TestCaseInTempDir
from samba.dcerpc import security
from unittest import TestCase
import uuid
from samba import param


class SamDBTestCase(TestCaseInTempDir):
    """Base-class for tests with a Sam Database.
    
    This is used by the Samba SamDB-tests, but e.g. also by the OpenChange
    provisioning tests (which need a Sam).
    """

    def setup_path(self, relpath):
        return os.path.join(find_setup_dir(), relpath)

    def setUp(self):
        super(SamDBTestCase, self).setUp()
        invocationid = str(uuid.uuid4())
        domaindn = "DC=COM,DC=EXAMPLE"
        self.domaindn = domaindn
        configdn = "CN=Configuration," + domaindn
        schemadn = "CN=Schema," + configdn
        domainguid = str(uuid.uuid4())
        policyguid = str(uuid.uuid4())
        creds = Credentials()
        creds.set_anonymous()
        domainsid = security.random_sid()
        hostguid = str(uuid.uuid4())
        path = os.path.join(self.tempdir, "samdb.ldb")
        session_info = system_session()
        
        hostname="foo"
        domain="EXAMPLE"
        dnsdomain="example.com" 
        serverrole="domain controller"

        smbconf = os.path.join(self.tempdir, "smb.conf")
        make_smbconf(smbconf, self.setup_path, hostname, domain, dnsdomain, 
                     serverrole, self.tempdir)

        self.lp = param.LoadParm()
        self.lp.load(smbconf)

        names = guess_names(lp=self.lp, hostname=hostname, 
                            domain=domain, dnsdomain=dnsdomain, 
                            serverrole=serverrole, 
                            domaindn=self.domaindn, configdn=configdn, 
                            schemadn=schemadn)
        setup_templatesdb(os.path.join(self.tempdir, "templates.ldb"), 
                          self.setup_path, session_info=session_info, lp=self.lp)
        self.samdb = setup_samdb(path, self.setup_path, session_info, creds, 
                                 self.lp, names, 
                                 lambda x: None, domainsid, 
                                 domainguid, 
                                 policyguid, False, "secret", 
                                 "secret", "secret", invocationid, 
                                 "secret", "domain controller")

    def tearDown(self):
        for f in ['templates.ldb', 'schema.ldb', 'configuration.ldb', 
                  'users.ldb', 'samdb.ldb', 'smb.conf']:
            os.remove(os.path.join(self.tempdir, f))
        super(SamDBTestCase, self).tearDown()


# disable this test till andrew works it out ...
class SamDBTests(SamDBTestCase):
    """Tests for the SamDB implementation."""

    print "samdb add_foreign disabled for now"
#    def test_add_foreign(self):
