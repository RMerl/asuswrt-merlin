#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This is unit with tests for LDAP access checks

import optparse
import sys
import base64
import re
import os
import copy
import time

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from ldb import (
    SCOPE_BASE, SCOPE_SUBTREE, LdbError, ERR_NO_SUCH_OBJECT)
from samba.dcerpc import security

from samba.auth import system_session
from samba import gensec
from samba.samdb import SamDB
from samba.credentials import Credentials
import samba.tests
from samba.tests import delete_force
from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("ldap [options] <host>")
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)

# use command line creds if available
credopts = options.CredentialsOptions(parser)
parser.add_option_group(credopts)
opts, args = parser.parse_args()

if len(args) < 1:
    parser.print_usage()
    sys.exit(1)

host = args[0]
lp = sambaopts.get_loadparm()
creds = credopts.get_credentials(lp)
creds.set_gensec_features(creds.get_gensec_features() | gensec.FEATURE_SEAL)
creds_machine = copy.deepcopy(creds)
creds_user1 = copy.deepcopy(creds)
creds_user2 = copy.deepcopy(creds)
creds_user3 = copy.deepcopy(creds)

class BindTests(samba.tests.TestCase):

    info_dc = None

    def setUp(self):
        super(BindTests, self).setUp()
        # fetch rootDSEs
        if self.info_dc is None:
            res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])
            self.assertEquals(len(res), 1)
            BindTests.info_dc = res[0]
        # cache some of RootDSE props
        self.schema_dn = self.info_dc["schemaNamingContext"][0]
        self.domain_dn = self.info_dc["defaultNamingContext"][0]
        self.config_dn = self.info_dc["configurationNamingContext"][0]
        self.computer_dn = "CN=centos53,CN=Computers,%s" % self.domain_dn
        self.password = "P@ssw0rd"
        self.username = "BindTestUser_" + time.strftime("%s", time.gmtime())

    def tearDown(self):
        super(BindTests, self).tearDown()

    def test_computer_account_bind(self):
        # create a computer acocount for the test
        delete_force(ldb, self.computer_dn)
        ldb.add_ldif("""
dn: """ + self.computer_dn + """
cn: CENTOS53
displayName: CENTOS53$
name: CENTOS53
sAMAccountName: CENTOS53$
countryCode: 0
objectClass: computer
objectClass: organizationalPerson
objectClass: person
objectClass: top
objectClass: user
codePage: 0
userAccountControl: 4096
dNSHostName: centos53.alabala.test
operatingSystemVersion: 5.2 (3790)
operatingSystem: Windows Server 2003
""")
        ldb.modify_ldif("""
dn: """ + self.computer_dn + """
changetype: modify
replace: unicodePwd
unicodePwd:: """ + base64.b64encode("\"P@ssw0rd\"".encode('utf-16-le')) + """
""")

        # do a simple bind and search with the machine account
        creds_machine.set_bind_dn(self.computer_dn)
        creds_machine.set_password(self.password)
        print "BindTest with: " + creds_machine.get_bind_dn()
        ldb_machine = samba.tests.connect_samdb(host, credentials=creds_machine,
                                                lp=lp, ldap_only=True)
        res = ldb_machine.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])

    def test_user_account_bind(self):
        # create user
        ldb.newuser(username=self.username, password=self.password)
        ldb_res = ldb.search(base=self.domain_dn,
                                      scope=SCOPE_SUBTREE,
                                      expression="(samAccountName=%s)" % self.username)
        self.assertEquals(len(ldb_res), 1)
        user_dn = ldb_res[0]["dn"]

        # do a simple bind and search with the user account in format user@realm
        creds_user1.set_bind_dn(self.username + "@" + creds.get_realm())
        creds_user1.set_password(self.password)
        print "BindTest with: " + creds_user1.get_bind_dn()
        ldb_user1 = samba.tests.connect_samdb(host, credentials=creds_user1,
                                              lp=lp, ldap_only=True)
        res = ldb_user1.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])

        # do a simple bind and search with the user account in format domain\user
        creds_user2.set_bind_dn(creds.get_domain() + "\\" + self.username)
        creds_user2.set_password(self.password)
        print "BindTest with: " + creds_user2.get_bind_dn()
        ldb_user2 = samba.tests.connect_samdb(host, credentials=creds_user2,
                                              lp=lp, ldap_only=True)
        res = ldb_user2.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])

        # do a simple bind and search with the user account DN
        creds_user3.set_bind_dn(str(user_dn))
        creds_user3.set_password(self.password)
        print "BindTest with: " + creds_user3.get_bind_dn()
        ldb_user3 = samba.tests.connect_samdb(host, credentials=creds_user3,
                                              lp=lp, ldap_only=True)
        res = ldb_user3.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])


ldb = samba.tests.connect_samdb(host, credentials=creds, lp=lp, ldap_only=True)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(BindTests)).wasSuccessful():
    rc = 1

sys.exit(rc)
