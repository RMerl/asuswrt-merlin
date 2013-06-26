#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This is unit with tests for LDAP access checks

import optparse
import sys
import base64
import re
sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options
from samba.join import dc_join

from ldb import (
    SCOPE_BASE, SCOPE_SUBTREE, LdbError, ERR_NO_SUCH_OBJECT,
    ERR_UNWILLING_TO_PERFORM, ERR_INSUFFICIENT_ACCESS_RIGHTS)
from ldb import ERR_CONSTRAINT_VIOLATION
from ldb import ERR_OPERATIONS_ERROR
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_REPLACE, FLAG_MOD_ADD
from samba.dcerpc import security, drsuapi, misc

from samba.auth import system_session
from samba import gensec, sd_utils
from samba.samdb import SamDB
from samba.credentials import Credentials
import samba.tests
from samba.tests import delete_force
from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("acl.py [options] <host>")
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)
parser.add_option_group(options.VersionOptions(parser))

# use command line creds if available
credopts = options.CredentialsOptions(parser)
parser.add_option_group(credopts)
opts, args = parser.parse_args()

if len(args) < 1:
    parser.print_usage()
    sys.exit(1)

host = args[0]
if not "://" in host:
    ldaphost = "ldap://%s" % host
else:
    ldaphost = host
    start = host.rindex("://")
    host = host.lstrip(start+3)

lp = sambaopts.get_loadparm()
creds = credopts.get_credentials(lp)
creds.set_gensec_features(creds.get_gensec_features() | gensec.FEATURE_SEAL)

#
# Tests start here
#

class AclTests(samba.tests.TestCase):

    def setUp(self):
        super(AclTests, self).setUp()
        self.ldb_admin = ldb
        self.base_dn = ldb.domain_dn()
        self.domain_sid = security.dom_sid(ldb.get_domain_sid())
        self.user_pass = "samba123@"
        self.configuration_dn = self.ldb_admin.get_config_basedn().get_linearized()
        self.sd_utils = sd_utils.SDUtils(ldb)
        #used for anonymous login
        self.creds_tmp = Credentials()
        self.creds_tmp.set_username("")
        self.creds_tmp.set_password("")
        self.creds_tmp.set_domain(creds.get_domain())
        self.creds_tmp.set_realm(creds.get_realm())
        self.creds_tmp.set_workstation(creds.get_workstation())
        print "baseDN: %s" % self.base_dn

    def get_user_dn(self, name):
        return "CN=%s,CN=Users,%s" % (name, self.base_dn)

    def get_ldb_connection(self, target_username, target_password):
        creds_tmp = Credentials()
        creds_tmp.set_username(target_username)
        creds_tmp.set_password(target_password)
        creds_tmp.set_domain(creds.get_domain())
        creds_tmp.set_realm(creds.get_realm())
        creds_tmp.set_workstation(creds.get_workstation())
        creds_tmp.set_gensec_features(creds_tmp.get_gensec_features()
                                      | gensec.FEATURE_SEAL)
        ldb_target = SamDB(url=ldaphost, credentials=creds_tmp, lp=lp)
        return ldb_target

    # Test if we have any additional groups for users than default ones
    def assert_user_no_group_member(self, username):
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" % self.get_user_dn(username))
        try:
            self.assertEqual(res[0]["memberOf"][0], "")
        except KeyError:
            pass
        else:
            self.fail()

#tests on ldap add operations
class AclAddTests(AclTests):

    def setUp(self):
        super(AclAddTests, self).setUp()
        # Domain admin that will be creator of OU parent-child structure
        self.usr_admin_owner = "acl_add_user1"
        # Second domain admin that will not be creator of OU parent-child structure
        self.usr_admin_not_owner = "acl_add_user2"
        # Regular user
        self.regular_user = "acl_add_user3"
        self.test_user1 = "test_add_user1"
        self.test_group1 = "test_add_group1"
        self.ou1 = "OU=test_add_ou1"
        self.ou2 = "OU=test_add_ou2,%s" % self.ou1
        self.ldb_admin.newuser(self.usr_admin_owner, self.user_pass)
        self.ldb_admin.newuser(self.usr_admin_not_owner, self.user_pass)
        self.ldb_admin.newuser(self.regular_user, self.user_pass)

        # add admins to the Domain Admins group
        self.ldb_admin.add_remove_group_members("Domain Admins", self.usr_admin_owner,
                       add_members_operation=True)
        self.ldb_admin.add_remove_group_members("Domain Admins", self.usr_admin_not_owner,
                       add_members_operation=True)

        self.ldb_owner = self.get_ldb_connection(self.usr_admin_owner, self.user_pass)
        self.ldb_notowner = self.get_ldb_connection(self.usr_admin_not_owner, self.user_pass)
        self.ldb_user = self.get_ldb_connection(self.regular_user, self.user_pass)

    def tearDown(self):
        super(AclAddTests, self).tearDown()
        delete_force(self.ldb_admin, "CN=%s,%s,%s" %
                          (self.test_user1, self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" %
                          (self.test_group1, self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "%s,%s" % (self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "%s,%s" % (self.ou1, self.base_dn))
        delete_force(self.ldb_admin, self.get_user_dn(self.usr_admin_owner))
        delete_force(self.ldb_admin, self.get_user_dn(self.usr_admin_not_owner))
        delete_force(self.ldb_admin, self.get_user_dn(self.regular_user))
        delete_force(self.ldb_admin, self.get_user_dn("test_add_anonymous"))

    # Make sure top OU is deleted (and so everything under it)
    def assert_top_ou_deleted(self):
        res = self.ldb_admin.search(self.base_dn,
            expression="(distinguishedName=%s,%s)" % (
                "OU=test_add_ou1", self.base_dn))
        self.assertEqual(len(res), 0)

    def test_add_u1(self):
        """Testing OU with the rights of Doman Admin not creator of the OU """
        self.assert_top_ou_deleted()
        # Change descriptor for top level OU
        self.ldb_owner.create_ou("OU=test_add_ou1," + self.base_dn)
        self.ldb_owner.create_ou("OU=test_add_ou2,OU=test_add_ou1," + self.base_dn)
        user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.usr_admin_not_owner))
        mod = "(D;CI;WPCC;;;%s)" % str(user_sid)
        self.sd_utils.dacl_add_ace("OU=test_add_ou1," + self.base_dn, mod)
        # Test user and group creation with another domain admin's credentials
        self.ldb_notowner.newuser(self.test_user1, self.user_pass, userou=self.ou2)
        self.ldb_notowner.newgroup("test_add_group1", groupou="OU=test_add_ou2,OU=test_add_ou1",
                                   grouptype=4)
        # Make sure we HAVE created the two objects -- user and group
        # !!! We should not be able to do that, but however beacuse of ACE ordering our inherited Deny ACE
        # !!! comes after explicit (A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA) that comes from somewhere
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s,%s)" % ("CN=test_add_user1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertTrue(len(res) > 0)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s,%s)" % ("CN=test_add_group1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertTrue(len(res) > 0)

    def test_add_u2(self):
        """Testing OU with the regular user that has no rights granted over the OU """
        self.assert_top_ou_deleted()
        # Create a parent-child OU structure with domain admin credentials
        self.ldb_owner.create_ou("OU=test_add_ou1," + self.base_dn)
        self.ldb_owner.create_ou("OU=test_add_ou2,OU=test_add_ou1," + self.base_dn)
        # Test user and group creation with regular user credentials
        try:
            self.ldb_user.newuser(self.test_user1, self.user_pass, userou=self.ou2)
            self.ldb_user.newgroup("test_add_group1", groupou="OU=test_add_ou2,OU=test_add_ou1",
                                   grouptype=4)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()
        # Make sure we HAVEN'T created any of two objects -- user or group
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s,%s)" % ("CN=test_add_user1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s,%s)" % ("CN=test_add_group1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertEqual(len(res), 0)

    def test_add_u3(self):
        """Testing OU with the rights of regular user granted the right 'Create User child objects' """
        self.assert_top_ou_deleted()
        # Change descriptor for top level OU
        self.ldb_owner.create_ou("OU=test_add_ou1," + self.base_dn)
        user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        mod = "(OA;CI;CC;bf967aba-0de6-11d0-a285-00aa003049e2;;%s)" % str(user_sid)
        self.sd_utils.dacl_add_ace("OU=test_add_ou1," + self.base_dn, mod)
        self.ldb_owner.create_ou("OU=test_add_ou2,OU=test_add_ou1," + self.base_dn)
        # Test user and group creation with granted user only to one of the objects
        self.ldb_user.newuser(self.test_user1, self.user_pass, userou=self.ou2, setpassword=False)
        try:
            self.ldb_user.newgroup("test_add_group1", groupou="OU=test_add_ou2,OU=test_add_ou1",
                                   grouptype=4)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()
        # Make sure we HAVE created the one of two objects -- user
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s,%s)" %
                ("CN=test_add_user1,OU=test_add_ou2,OU=test_add_ou1",
                    self.base_dn))
        self.assertNotEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s,%s)" %
                ("CN=test_add_group1,OU=test_add_ou2,OU=test_add_ou1",
                    self.base_dn) )
        self.assertEqual(len(res), 0)

    def test_add_u4(self):
        """ 4 Testing OU with the rights of Doman Admin creator of the OU"""
        self.assert_top_ou_deleted()
        self.ldb_owner.create_ou("OU=test_add_ou1," + self.base_dn)
        self.ldb_owner.create_ou("OU=test_add_ou2,OU=test_add_ou1," + self.base_dn)
        self.ldb_owner.newuser(self.test_user1, self.user_pass, userou=self.ou2)
        self.ldb_owner.newgroup("test_add_group1", groupou="OU=test_add_ou2,OU=test_add_ou1",
                                 grouptype=4)
        # Make sure we have successfully created the two objects -- user and group
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s,%s)" % ("CN=test_add_user1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertTrue(len(res) > 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s,%s)" % ("CN=test_add_group1,OU=test_add_ou2,OU=test_add_ou1", self.base_dn))
        self.assertTrue(len(res) > 0)

    def test_add_anonymous(self):
        """Test add operation with anonymous user"""
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        try:
            anonymous.newuser("test_add_anonymous", self.user_pass)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()

#tests on ldap modify operations
class AclModifyTests(AclTests):

    def setUp(self):
        super(AclModifyTests, self).setUp()
        self.user_with_wp = "acl_mod_user1"
        self.user_with_sm = "acl_mod_user2"
        self.user_with_group_sm = "acl_mod_user3"
        self.ldb_admin.newuser(self.user_with_wp, self.user_pass)
        self.ldb_admin.newuser(self.user_with_sm, self.user_pass)
        self.ldb_admin.newuser(self.user_with_group_sm, self.user_pass)
        self.ldb_user = self.get_ldb_connection(self.user_with_wp, self.user_pass)
        self.ldb_user2 = self.get_ldb_connection(self.user_with_sm, self.user_pass)
        self.ldb_user3 = self.get_ldb_connection(self.user_with_group_sm, self.user_pass)
        self.user_sid = self.sd_utils.get_object_sid( self.get_user_dn(self.user_with_wp))
        self.ldb_admin.newgroup("test_modify_group2", grouptype=4)
        self.ldb_admin.newgroup("test_modify_group3", grouptype=4)
        self.ldb_admin.newuser("test_modify_user2", self.user_pass)

    def tearDown(self):
        super(AclModifyTests, self).tearDown()
        delete_force(self.ldb_admin, self.get_user_dn("test_modify_user1"))
        delete_force(self.ldb_admin, "CN=test_modify_group1,CN=Users," + self.base_dn)
        delete_force(self.ldb_admin, "CN=test_modify_group2,CN=Users," + self.base_dn)
        delete_force(self.ldb_admin, "CN=test_modify_group3,CN=Users," + self.base_dn)
        delete_force(self.ldb_admin, "OU=test_modify_ou1," + self.base_dn)
        delete_force(self.ldb_admin, self.get_user_dn(self.user_with_wp))
        delete_force(self.ldb_admin, self.get_user_dn(self.user_with_sm))
        delete_force(self.ldb_admin, self.get_user_dn(self.user_with_group_sm))
        delete_force(self.ldb_admin, self.get_user_dn("test_modify_user2"))
        delete_force(self.ldb_admin, self.get_user_dn("test_anonymous"))

    def test_modify_u1(self):
        """5 Modify one attribute if you have DS_WRITE_PROPERTY for it"""
        mod = "(OA;;WP;bf967953-0de6-11d0-a285-00aa003049e2;;%s)" % str(self.user_sid)
        # First test object -- User
        print "Testing modify on User object"
        self.ldb_admin.newuser("test_modify_user1", self.user_pass)
        self.sd_utils.dacl_add_ace(self.get_user_dn("test_modify_user1"), mod)
        ldif = """
dn: """ + self.get_user_dn("test_modify_user1") + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % self.get_user_dn("test_modify_user1"))
        self.assertEqual(res[0]["displayName"][0], "test_changed")
        # Second test object -- Group
        print "Testing modify on Group object"
        self.ldb_admin.newgroup("test_modify_group1", grouptype=4)
        self.sd_utils.dacl_add_ace("CN=test_modify_group1,CN=Users," + self.base_dn, mod)
        ldif = """
dn: CN=test_modify_group1,CN=Users,""" + self.base_dn + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" % str("CN=test_modify_group1,CN=Users," + self.base_dn))
        self.assertEqual(res[0]["displayName"][0], "test_changed")
        # Third test object -- Organizational Unit
        print "Testing modify on OU object"
        #delete_force(self.ldb_admin, "OU=test_modify_ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=test_modify_ou1," + self.base_dn)
        self.sd_utils.dacl_add_ace("OU=test_modify_ou1," + self.base_dn, mod)
        ldif = """
dn: OU=test_modify_ou1,""" + self.base_dn + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" % str("OU=test_modify_ou1," + self.base_dn))
        self.assertEqual(res[0]["displayName"][0], "test_changed")

    def test_modify_u2(self):
        """6 Modify two attributes as you have DS_WRITE_PROPERTY granted only for one of them"""
        mod = "(OA;;WP;bf967953-0de6-11d0-a285-00aa003049e2;;%s)" % str(self.user_sid)
        # First test object -- User
        print "Testing modify on User object"
        #delete_force(self.ldb_admin, self.get_user_dn("test_modify_user1"))
        self.ldb_admin.newuser("test_modify_user1", self.user_pass)
        self.sd_utils.dacl_add_ace(self.get_user_dn("test_modify_user1"), mod)
        # Modify on attribute you have rights for
        ldif = """
dn: """ + self.get_user_dn("test_modify_user1") + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" %
                self.get_user_dn("test_modify_user1"))
        self.assertEqual(res[0]["displayName"][0], "test_changed")
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: """ + self.get_user_dn("test_modify_user1") + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()
        # Second test object -- Group
        print "Testing modify on Group object"
        self.ldb_admin.newgroup("test_modify_group1", grouptype=4)
        self.sd_utils.dacl_add_ace("CN=test_modify_group1,CN=Users," + self.base_dn, mod)
        ldif = """
dn: CN=test_modify_group1,CN=Users,""" + self.base_dn + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" %
                str("CN=test_modify_group1,CN=Users," + self.base_dn))
        self.assertEqual(res[0]["displayName"][0], "test_changed")
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: CN=test_modify_group1,CN=Users,""" + self.base_dn + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()
        # Second test object -- Organizational Unit
        print "Testing modify on OU object"
        self.ldb_admin.create_ou("OU=test_modify_ou1," + self.base_dn)
        self.sd_utils.dacl_add_ace("OU=test_modify_ou1," + self.base_dn, mod)
        ldif = """
dn: OU=test_modify_ou1,""" + self.base_dn + """
changetype: modify
replace: displayName
displayName: test_changed"""
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % str("OU=test_modify_ou1,"
                    + self.base_dn))
        self.assertEqual(res[0]["displayName"][0], "test_changed")
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: OU=test_modify_ou1,""" + self.base_dn + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()

    def test_modify_u3(self):
        """7 Modify one attribute as you have no what so ever rights granted"""
        # First test object -- User
        print "Testing modify on User object"
        self.ldb_admin.newuser("test_modify_user1", self.user_pass)
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: """ + self.get_user_dn("test_modify_user1") + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()

        # Second test object -- Group
        print "Testing modify on Group object"
        self.ldb_admin.newgroup("test_modify_group1", grouptype=4)
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: CN=test_modify_group1,CN=Users,""" + self.base_dn + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()

        # Second test object -- Organizational Unit
        print "Testing modify on OU object"
        #delete_force(self.ldb_admin, "OU=test_modify_ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=test_modify_ou1," + self.base_dn)
        # Modify on attribute you do not have rights for granted
        ldif = """
dn: OU=test_modify_ou1,""" + self.base_dn + """
changetype: modify
replace: url
url: www.samba.org"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()


    def test_modify_u4(self):
        """11 Grant WP to PRINCIPAL_SELF and test modify"""
        ldif = """
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
add: adminDescription
adminDescription: blah blah blah"""
        try:
            self.ldb_user.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()

        mod = "(OA;;WP;bf967919-0de6-11d0-a285-00aa003049e2;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        # Modify on attribute you have rights for
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" \
                                    % self.get_user_dn(self.user_with_wp), attrs=["adminDescription"] )
        self.assertEqual(res[0]["adminDescription"][0], "blah blah blah")

    def test_modify_u5(self):
        """12 test self membership"""
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
add: Member
Member: """ +  self.get_user_dn(self.user_with_sm)
#the user has no rights granted, this should fail
        try:
            self.ldb_user2.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This 'modify' operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()

#grant self-membership, should be able to add himself
        user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.user_with_sm))
        mod = "(OA;;SW;bf9679c0-0de6-11d0-a285-00aa003049e2;;%s)" % str(user_sid)
        self.sd_utils.dacl_add_ace("CN=test_modify_group2,CN=Users," + self.base_dn, mod)
        self.ldb_user2.modify_ldif(ldif)
        res = self.ldb_admin.search( self.base_dn, expression="(distinguishedName=%s)" \
                                    % ("CN=test_modify_group2,CN=Users," + self.base_dn), attrs=["Member"])
        self.assertEqual(res[0]["Member"][0], self.get_user_dn(self.user_with_sm))
#but not other users
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
add: Member
Member: CN=test_modify_user2,CN=Users,""" + self.base_dn
        try:
            self.ldb_user2.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_modify_u6(self):
        """13 test self membership"""
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
add: Member
Member: """ +  self.get_user_dn(self.user_with_sm) + """
Member: CN=test_modify_user2,CN=Users,""" + self.base_dn

#grant self-membership, should be able to add himself  but not others at the same time
        user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.user_with_sm))
        mod = "(OA;;SW;bf9679c0-0de6-11d0-a285-00aa003049e2;;%s)" % str(user_sid)
        self.sd_utils.dacl_add_ace("CN=test_modify_group2,CN=Users," + self.base_dn, mod)
        try:
            self.ldb_user2.modify_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_modify_u7(self):
        """13 User with WP modifying Member"""
#a second user is given write property permission
        user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.user_with_wp))
        mod = "(A;;WP;;;%s)" % str(user_sid)
        self.sd_utils.dacl_add_ace("CN=test_modify_group2,CN=Users," + self.base_dn, mod)
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
add: Member
Member: """ +  self.get_user_dn(self.user_with_wp)
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search( self.base_dn, expression="(distinguishedName=%s)" \
                                    % ("CN=test_modify_group2,CN=Users," + self.base_dn), attrs=["Member"])
        self.assertEqual(res[0]["Member"][0], self.get_user_dn(self.user_with_wp))
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
delete: Member"""
        self.ldb_user.modify_ldif(ldif)
        ldif = """
dn: CN=test_modify_group2,CN=Users,""" + self.base_dn + """
changetype: modify
add: Member
Member: CN=test_modify_user2,CN=Users,""" + self.base_dn
        self.ldb_user.modify_ldif(ldif)
        res = self.ldb_admin.search( self.base_dn, expression="(distinguishedName=%s)" \
                                    % ("CN=test_modify_group2,CN=Users," + self.base_dn), attrs=["Member"])
        self.assertEqual(res[0]["Member"][0], "CN=test_modify_user2,CN=Users," + self.base_dn)

    def test_modify_anonymous(self):
        """Test add operation with anonymous user"""
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        self.ldb_admin.newuser("test_anonymous", "samba123@")
        m = Message()
        m.dn = Dn(anonymous, self.get_user_dn("test_anonymous"))

        m["description"] = MessageElement("sambauser2",
                                          FLAG_MOD_ADD,
                                          "description")
        try:
            anonymous.modify(m)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()

#enable these when we have search implemented
class AclSearchTests(AclTests):

    def setUp(self):
        super(AclSearchTests, self).setUp()
        self.u1 = "search_u1"
        self.u2 = "search_u2"
        self.u3 = "search_u3"
        self.group1 = "group1"
        self.ldb_admin.newuser(self.u1, self.user_pass)
        self.ldb_admin.newuser(self.u2, self.user_pass)
        self.ldb_admin.newuser(self.u3, self.user_pass)
        self.ldb_admin.newgroup(self.group1, grouptype=-2147483646)
        self.ldb_admin.add_remove_group_members(self.group1, self.u2,
                                                add_members_operation=True)
        self.ldb_user = self.get_ldb_connection(self.u1, self.user_pass)
        self.ldb_user2 = self.get_ldb_connection(self.u2, self.user_pass)
        self.ldb_user3 = self.get_ldb_connection(self.u3, self.user_pass)
        self.full_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                          Dn(self.ldb_admin,  "OU=ou1," + self.base_dn),
                          Dn(self.ldb_admin,  "OU=ou3,OU=ou2,OU=ou1," + self.base_dn),
                          Dn(self.ldb_admin,  "OU=ou4,OU=ou2,OU=ou1," + self.base_dn),
                          Dn(self.ldb_admin,  "OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn),
                          Dn(self.ldb_admin,  "OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn)]
        self.user_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.u1))
        self.group_sid = self.sd_utils.get_object_sid(self.get_user_dn(self.group1))

    def create_clean_ou(self, object_dn):
        """ Base repeating setup for unittests to follow """
        res = self.ldb_admin.search(base=self.base_dn, scope=SCOPE_SUBTREE, \
                expression="distinguishedName=%s" % object_dn)
        # Make sure top testing OU has been deleted before starting the test
        self.assertEqual(len(res), 0)
        self.ldb_admin.create_ou(object_dn)
        desc_sddl = self.sd_utils.get_sd_as_sddl(object_dn)
        # Make sure there are inheritable ACEs initially
        self.assertTrue("CI" in desc_sddl or "OI" in desc_sddl)
        # Find and remove all inherit ACEs
        res = re.findall("\(.*?\)", desc_sddl)
        res = [x for x in res if ("CI" in x) or ("OI" in x)]
        for x in res:
            desc_sddl = desc_sddl.replace(x, "")
        # Add flag 'protected' in both DACL and SACL so no inherit ACEs
        # can propagate from above
        # remove SACL, we are not interested
        desc_sddl = desc_sddl.replace(":AI", ":AIP")
        self.sd_utils.modify_sd_on_dn(object_dn, desc_sddl)
        # Verify all inheritable ACEs are gone
        desc_sddl = self.sd_utils.get_sd_as_sddl(object_dn)
        self.assertFalse("CI" in desc_sddl)
        self.assertFalse("OI" in desc_sddl)

    def tearDown(self):
        super(AclSearchTests, self).tearDown()
        delete_force(self.ldb_admin, "OU=test_search_ou2,OU=test_search_ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=test_search_ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou4,OU=ou2,OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou3,OU=ou2,OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou2,OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, "OU=ou1," + self.base_dn)
        delete_force(self.ldb_admin, self.get_user_dn("search_u1"))
        delete_force(self.ldb_admin, self.get_user_dn("search_u2"))
        delete_force(self.ldb_admin, self.get_user_dn("search_u3"))
        delete_force(self.ldb_admin, self.get_user_dn("group1"))

    def test_search_anonymous1(self):
        """Verify access of rootDSE with the correct request"""
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        res = anonymous.search("", expression="(objectClass=*)", scope=SCOPE_BASE)
        self.assertEquals(len(res), 1)
        #verify some of the attributes
        #dont care about values
        self.assertTrue("ldapServiceName" in res[0])
        self.assertTrue("namingContexts" in res[0])
        self.assertTrue("isSynchronized" in res[0])
        self.assertTrue("dsServiceName" in res[0])
        self.assertTrue("supportedSASLMechanisms" in res[0])
        self.assertTrue("isGlobalCatalogReady" in res[0])
        self.assertTrue("domainControllerFunctionality" in res[0])
        self.assertTrue("serverName" in res[0])

    def test_search_anonymous2(self):
        """Make sure we cannot access anything else"""
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        try:
            res = anonymous.search("", expression="(objectClass=*)", scope=SCOPE_SUBTREE)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()
        try:
            res = anonymous.search(self.base_dn, expression="(objectClass=*)", scope=SCOPE_SUBTREE)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()
        try:
            res = anonymous.search("CN=Configuration," + self.base_dn, expression="(objectClass=*)",
                                        scope=SCOPE_SUBTREE)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()

    def test_search_anonymous3(self):
        """Set dsHeuristics and repeat"""
        self.ldb_admin.set_dsheuristics("0000002")
        self.ldb_admin.create_ou("OU=test_search_ou1," + self.base_dn)
        mod = "(A;CI;LC;;;AN)"
        self.sd_utils.dacl_add_ace("OU=test_search_ou1," + self.base_dn, mod)
        self.ldb_admin.create_ou("OU=test_search_ou2,OU=test_search_ou1," + self.base_dn)
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        res = anonymous.search("OU=test_search_ou2,OU=test_search_ou1," + self.base_dn,
                               expression="(objectClass=*)", scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1)
        self.assertTrue("dn" in res[0])
        self.assertTrue(res[0]["dn"] == Dn(self.ldb_admin,
                                           "OU=test_search_ou2,OU=test_search_ou1," + self.base_dn))
        res = anonymous.search("CN=Configuration," + self.base_dn, expression="(objectClass=*)",
                               scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1)
        self.assertTrue("dn" in res[0])
        self.assertTrue(res[0]["dn"] == Dn(self.ldb_admin, self.configuration_dn))

    def test_search1(self):
        """Make sure users can see us if given LC to user and group"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        mod = "(A;;LC;;;%s)(A;;LC;;;%s)" % (str(self.user_sid), str(self.group_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        tmp_desc = security.descriptor.from_sddl("D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)" + mod,
                                                 self.domain_sid)
        self.ldb_admin.create_ou("OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)

        #regular users must see only ou1 and ou2
        res = self.ldb_user3.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 2)
        ok_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou1," + self.base_dn)]

        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

        #these users should see all ous
        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 6)
        res_list = [ x["dn"] for x in res if x["dn"] in self.full_list ]
        self.assertEquals(sorted(res_list), sorted(self.full_list))

        res = self.ldb_user2.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 6)
        res_list = [ x["dn"] for x in res if x["dn"] in self.full_list ]
        self.assertEquals(sorted(res_list), sorted(self.full_list))

    def test_search2(self):
        """Make sure users can't see us if access is explicitly denied"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=ou2,OU=ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=ou3,OU=ou2,OU=ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=ou4,OU=ou2,OU=ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn)
        self.ldb_admin.create_ou("OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn)
        mod = "(D;;LC;;;%s)(D;;LC;;;%s)" % (str(self.user_sid), str(self.group_sid)) 
        self.sd_utils.dacl_add_ace("OU=ou2,OU=ou1," + self.base_dn, mod)
        res = self.ldb_user3.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        #this user should see all ous
        res_list = [ x["dn"] for x in res if x["dn"] in self.full_list ]
        self.assertEquals(sorted(res_list), sorted(self.full_list))

        #these users should see ou1, 2, 5 and 6 but not 3 and 4
        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        ok_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn)]
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

        res = self.ldb_user2.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 4)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

    def test_search3(self):
        """Make sure users can't see ous if access is explicitly denied - 2"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        mod = "(A;CI;LC;;;%s)(A;CI;LC;;;%s)" % (str(self.user_sid), str(self.group_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        tmp_desc = security.descriptor.from_sddl("D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)" + mod,
                                                 self.domain_sid)
        self.ldb_admin.create_ou("OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_admin.create_ou("OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)

        print "Testing correct behavior on nonaccessible search base"
        try:
             self.ldb_user3.search("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                   scope=SCOPE_BASE)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)
        else:
            self.fail()

        mod = "(D;;LC;;;%s)(D;;LC;;;%s)" % (str(self.user_sid), str(self.group_sid))
        self.sd_utils.dacl_add_ace("OU=ou2,OU=ou1," + self.base_dn, mod)

        ok_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou1," + self.base_dn)]

        res = self.ldb_user3.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

        ok_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn)]

        #should not see ou3 and ou4, but should see ou5 and ou6
        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 4)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

        res = self.ldb_user2.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 4)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

    def test_search4(self):
        """There is no difference in visibility if the user is also creator"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        mod = "(A;CI;CC;;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        tmp_desc = security.descriptor.from_sddl("D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)" + mod,
                                                 self.domain_sid)
        self.ldb_user.create_ou("OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_user.create_ou("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_user.create_ou("OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_user.create_ou("OU=ou5,OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_user.create_ou("OU=ou6,OU=ou4,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)

        ok_list = [Dn(self.ldb_admin,  "OU=ou2,OU=ou1," + self.base_dn),
                   Dn(self.ldb_admin,  "OU=ou1," + self.base_dn)]
        res = self.ldb_user3.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 2)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 2)
        res_list = [ x["dn"] for x in res if x["dn"] in ok_list ]
        self.assertEquals(sorted(res_list), sorted(ok_list))

    def test_search5(self):
        """Make sure users can see only attributes they are allowed to see"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        mod = "(A;CI;LC;;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        tmp_desc = security.descriptor.from_sddl("D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)" + mod,
                                                 self.domain_sid)
        self.ldb_admin.create_ou("OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        # assert user can only see dn
        res = self.ldb_user.search("OU=ou2,OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        ok_list = ['dn']
        self.assertEquals(len(res), 1)
        res_list = res[0].keys()
        self.assertEquals(res_list, ok_list)

        res = self.ldb_user.search("OU=ou2,OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_BASE, attrs=["ou"])

        self.assertEquals(len(res), 1)
        res_list = res[0].keys()
        self.assertEquals(res_list, ok_list)

        #give read property on ou and assert user can only see dn and ou
        mod = "(OA;;RP;bf9679f0-0de6-11d0-a285-00aa003049e2;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        self.sd_utils.dacl_add_ace("OU=ou2,OU=ou1," + self.base_dn, mod)
        res = self.ldb_user.search("OU=ou2,OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)
        ok_list = ['dn', 'ou']
        self.assertEquals(len(res), 1)
        res_list = res[0].keys()
        self.assertEquals(sorted(res_list), sorted(ok_list))

        #give read property on Public Information and assert user can see ou and other members
        mod = "(OA;;RP;e48d0154-bcf8-11d1-8702-00c04fb96050;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        self.sd_utils.dacl_add_ace("OU=ou2,OU=ou1," + self.base_dn, mod)
        res = self.ldb_user.search("OU=ou2,OU=ou1," + self.base_dn, expression="(objectClass=*)",
                                    scope=SCOPE_SUBTREE)

        ok_list = ['dn', 'objectClass', 'ou', 'distinguishedName', 'name', 'objectGUID', 'objectCategory']
        res_list = res[0].keys()
        self.assertEquals(sorted(res_list), sorted(ok_list))

    def test_search6(self):
        """If an attribute that cannot be read is used in a filter, it is as if the attribute does not exist"""
        self.create_clean_ou("OU=ou1," + self.base_dn)
        mod = "(A;CI;LCCC;;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou1," + self.base_dn, mod)
        tmp_desc = security.descriptor.from_sddl("D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)" + mod,
                                                 self.domain_sid)
        self.ldb_admin.create_ou("OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)
        self.ldb_user.create_ou("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, sd=tmp_desc)

        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(ou=ou3)",
                                    scope=SCOPE_SUBTREE)
        #nothing should be returned as ou is not accessible
        self.assertEquals(len(res), 0)

        #give read property on ou and assert user can only see dn and ou
        mod = "(OA;;RP;bf9679f0-0de6-11d0-a285-00aa003049e2;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou3,OU=ou2,OU=ou1," + self.base_dn, mod)
        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(ou=ou3)",
                                    scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1)
        ok_list = ['dn', 'ou']
        res_list = res[0].keys()
        self.assertEquals(sorted(res_list), sorted(ok_list))

        #give read property on Public Information and assert user can see ou and other members
        mod = "(OA;;RP;e48d0154-bcf8-11d1-8702-00c04fb96050;;%s)" % (str(self.user_sid))
        self.sd_utils.dacl_add_ace("OU=ou2,OU=ou1," + self.base_dn, mod)
        res = self.ldb_user.search("OU=ou1," + self.base_dn, expression="(ou=ou2)",
                                   scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1)
        ok_list = ['dn', 'objectClass', 'ou', 'distinguishedName', 'name', 'objectGUID', 'objectCategory']
        res_list = res[0].keys()
        self.assertEquals(sorted(res_list), sorted(ok_list))

#tests on ldap delete operations
class AclDeleteTests(AclTests):

    def setUp(self):
        super(AclDeleteTests, self).setUp()
        self.regular_user = "acl_delete_user1"
        # Create regular user
        self.ldb_admin.newuser(self.regular_user, self.user_pass)
        self.ldb_user = self.get_ldb_connection(self.regular_user, self.user_pass)

    def tearDown(self):
        super(AclDeleteTests, self).tearDown()
        delete_force(self.ldb_admin, self.get_user_dn("test_delete_user1"))
        delete_force(self.ldb_admin, self.get_user_dn(self.regular_user))
        delete_force(self.ldb_admin, self.get_user_dn("test_anonymous"))

    def test_delete_u1(self):
        """User is prohibited by default to delete another User object"""
        # Create user that we try to delete
        self.ldb_admin.newuser("test_delete_user1", self.user_pass)
        # Here delete User object should ALWAYS through exception
        try:
            self.ldb_user.delete(self.get_user_dn("test_delete_user1"))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_delete_u2(self):
        """User's group has RIGHT_DELETE to another User object"""
        user_dn = self.get_user_dn("test_delete_user1")
        # Create user that we try to delete
        self.ldb_admin.newuser("test_delete_user1", self.user_pass)
        mod = "(A;;SD;;;AU)"
        self.sd_utils.dacl_add_ace(user_dn, mod)
        # Try to delete User object
        self.ldb_user.delete(user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)

    def test_delete_u3(self):
        """User indentified by SID has RIGHT_DELETE to another User object"""
        user_dn = self.get_user_dn("test_delete_user1")
        # Create user that we try to delete
        self.ldb_admin.newuser("test_delete_user1", self.user_pass)
        mod = "(A;;SD;;;%s)" % self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        self.sd_utils.dacl_add_ace(user_dn, mod)
        # Try to delete User object
        self.ldb_user.delete(user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)

    def test_delete_anonymous(self):
        """Test add operation with anonymous user"""
        anonymous = SamDB(url=ldaphost, credentials=self.creds_tmp, lp=lp)
        self.ldb_admin.newuser("test_anonymous", "samba123@")

        try:
            anonymous.delete(self.get_user_dn("test_anonymous"))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OPERATIONS_ERROR)
        else:
            self.fail()

#tests on ldap rename operations
class AclRenameTests(AclTests):

    def setUp(self):
        super(AclRenameTests, self).setUp()
        self.regular_user = "acl_rename_user1"
        self.ou1 = "OU=test_rename_ou1"
        self.ou2 = "OU=test_rename_ou2"
        self.ou3 = "OU=test_rename_ou3,%s" % self.ou2
        self.testuser1 = "test_rename_user1"
        self.testuser2 = "test_rename_user2"
        self.testuser3 = "test_rename_user3"
        self.testuser4 = "test_rename_user4"
        self.testuser5 = "test_rename_user5"
        # Create regular user
        self.ldb_admin.newuser(self.regular_user, self.user_pass)
        self.ldb_user = self.get_ldb_connection(self.regular_user, self.user_pass)

    def tearDown(self):
        super(AclRenameTests, self).tearDown()
        # Rename OU3
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser1, self.ou3, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser2, self.ou3, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser5, self.ou3, self.base_dn))
        delete_force(self.ldb_admin, "%s,%s" % (self.ou3, self.base_dn))
        # Rename OU2
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser1, self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser2, self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser5, self.ou2, self.base_dn))
        delete_force(self.ldb_admin, "%s,%s" % (self.ou2, self.base_dn))
        # Rename OU1
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser1, self.ou1, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser2, self.ou1, self.base_dn))
        delete_force(self.ldb_admin, "CN=%s,%s,%s" % (self.testuser5, self.ou1, self.base_dn))
        delete_force(self.ldb_admin, "OU=test_rename_ou3,%s,%s" % (self.ou1, self.base_dn))
        delete_force(self.ldb_admin, "%s,%s" % (self.ou1, self.base_dn))
        delete_force(self.ldb_admin, self.get_user_dn(self.regular_user))

    def test_rename_u1(self):
        """Regular user fails to rename 'User object' within single OU"""
        # Create OU structure
        self.ldb_admin.create_ou("OU=test_rename_ou1," + self.base_dn)
        self.ldb_admin.newuser(self.testuser1, self.user_pass, userou=self.ou1)
        try:
            self.ldb_user.rename("CN=%s,%s,%s" % (self.testuser1, self.ou1, self.base_dn), \
                                     "CN=%s,%s,%s" % (self.testuser5, self.ou1, self.base_dn))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_rename_u2(self):
        """Grant WRITE_PROPERTY to AU so regular user can rename 'User object' within single OU"""
        ou_dn = "OU=test_rename_ou1," + self.base_dn
        user_dn = "CN=test_rename_user1," + ou_dn
        rename_user_dn = "CN=test_rename_user5," + ou_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou_dn)
        self.ldb_admin.newuser(self.testuser1, self.user_pass, userou=self.ou1)
        mod = "(A;;WP;;;AU)"
        self.sd_utils.dacl_add_ace(user_dn, mod)
        # Rename 'User object' having WP to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u3(self):
        """Test rename with rights granted to 'User object' SID"""
        ou_dn = "OU=test_rename_ou1," + self.base_dn
        user_dn = "CN=test_rename_user1," + ou_dn
        rename_user_dn = "CN=test_rename_user5," + ou_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou_dn)
        self.ldb_admin.newuser(self.testuser1, self.user_pass, userou=self.ou1)
        sid = self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        mod = "(A;;WP;;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(user_dn, mod)
        # Rename 'User object' having WP to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u4(self):
        """Rename 'User object' cross OU with WP, SD and CC right granted on reg. user to AU"""
        ou1_dn = "OU=test_rename_ou1," + self.base_dn
        ou2_dn = "OU=test_rename_ou2," + self.base_dn
        user_dn = "CN=test_rename_user2," + ou1_dn
        rename_user_dn = "CN=test_rename_user5," + ou2_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou1_dn)
        self.ldb_admin.create_ou(ou2_dn)
        self.ldb_admin.newuser(self.testuser2, self.user_pass, userou=self.ou1)
        mod = "(A;;WPSD;;;AU)"
        self.sd_utils.dacl_add_ace(user_dn, mod)
        mod = "(A;;CC;;;AU)"
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        # Rename 'User object' having SD and CC to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u5(self):
        """Test rename with rights granted to 'User object' SID"""
        ou1_dn = "OU=test_rename_ou1," + self.base_dn
        ou2_dn = "OU=test_rename_ou2," + self.base_dn
        user_dn = "CN=test_rename_user2," + ou1_dn
        rename_user_dn = "CN=test_rename_user5," + ou2_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou1_dn)
        self.ldb_admin.create_ou(ou2_dn)
        self.ldb_admin.newuser(self.testuser2, self.user_pass, userou=self.ou1)
        sid = self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        mod = "(A;;WPSD;;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(user_dn, mod)
        mod = "(A;;CC;;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        # Rename 'User object' having SD and CC to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u6(self):
        """Rename 'User object' cross OU with WP, DC and CC right granted on OU & user to AU"""
        ou1_dn = "OU=test_rename_ou1," + self.base_dn
        ou2_dn = "OU=test_rename_ou2," + self.base_dn
        user_dn = "CN=test_rename_user2," + ou1_dn
        rename_user_dn = "CN=test_rename_user2," + ou2_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou1_dn)
        self.ldb_admin.create_ou(ou2_dn)
        #mod = "(A;CI;DCWP;;;AU)"
        mod = "(A;;DC;;;AU)"
        self.sd_utils.dacl_add_ace(ou1_dn, mod)
        mod = "(A;;CC;;;AU)"
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        self.ldb_admin.newuser(self.testuser2, self.user_pass, userou=self.ou1)
        mod = "(A;;WP;;;AU)"
        self.sd_utils.dacl_add_ace(user_dn, mod)
        # Rename 'User object' having SD and CC to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u7(self):
        """Rename 'User object' cross OU (second level) with WP, DC and CC right granted on OU to AU"""
        ou1_dn = "OU=test_rename_ou1," + self.base_dn
        ou2_dn = "OU=test_rename_ou2," + self.base_dn
        ou3_dn = "OU=test_rename_ou3," + ou2_dn
        user_dn = "CN=test_rename_user2," + ou1_dn
        rename_user_dn = "CN=test_rename_user5," + ou3_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou1_dn)
        self.ldb_admin.create_ou(ou2_dn)
        self.ldb_admin.create_ou(ou3_dn)
        mod = "(A;CI;WPDC;;;AU)"
        self.sd_utils.dacl_add_ace(ou1_dn, mod)
        mod = "(A;;CC;;;AU)"
        self.sd_utils.dacl_add_ace(ou3_dn, mod)
        self.ldb_admin.newuser(self.testuser2, self.user_pass, userou=self.ou1)
        # Rename 'User object' having SD and CC to AU
        self.ldb_user.rename(user_dn, rename_user_dn)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % user_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn,
                expression="(distinguishedName=%s)" % rename_user_dn)
        self.assertNotEqual(len(res), 0)

    def test_rename_u8(self):
        """Test rename on an object with and without modify access on the RDN attribute"""
        ou1_dn = "OU=test_rename_ou1," + self.base_dn
        ou2_dn = "OU=test_rename_ou2," + ou1_dn
        ou3_dn = "OU=test_rename_ou3," + ou1_dn
        # Create OU structure
        self.ldb_admin.create_ou(ou1_dn)
        self.ldb_admin.create_ou(ou2_dn)
        sid = self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        mod = "(OA;;WP;bf967a0e-0de6-11d0-a285-00aa003049e2;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        mod = "(OD;;WP;bf9679f0-0de6-11d0-a285-00aa003049e2;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        try:
            self.ldb_user.rename(ou2_dn, ou3_dn)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            # This rename operation should always throw ERR_INSUFFICIENT_ACCESS_RIGHTS
            self.fail()
        sid = self.sd_utils.get_object_sid(self.get_user_dn(self.regular_user))
        mod = "(A;;WP;bf9679f0-0de6-11d0-a285-00aa003049e2;;%s)" % str(sid)
        self.sd_utils.dacl_add_ace(ou2_dn, mod)
        self.ldb_user.rename(ou2_dn, ou3_dn)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" % ou2_dn)
        self.assertEqual(len(res), 0)
        res = self.ldb_admin.search(self.base_dn, expression="(distinguishedName=%s)" % ou3_dn)
        self.assertNotEqual(len(res), 0)

#tests on Control Access Rights
class AclCARTests(AclTests):

    def setUp(self):
        super(AclCARTests, self).setUp()
        self.user_with_wp = "acl_car_user1"
        self.user_with_pc = "acl_car_user2"
        self.ldb_admin.newuser(self.user_with_wp, self.user_pass)
        self.ldb_admin.newuser(self.user_with_pc, self.user_pass)
        self.ldb_user = self.get_ldb_connection(self.user_with_wp, self.user_pass)
        self.ldb_user2 = self.get_ldb_connection(self.user_with_pc, self.user_pass)

    def tearDown(self):
        super(AclCARTests, self).tearDown()
        delete_force(self.ldb_admin, self.get_user_dn(self.user_with_wp))
        delete_force(self.ldb_admin, self.get_user_dn(self.user_with_pc))

    def test_change_password1(self):
        """Try a password change operation without any CARs given"""
        #users have change password by default - remove for negative testing
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;WD)", "")
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;PS)", "")
        self.sd_utils.modify_sd_on_dn(self.get_user_dn(self.user_with_wp), sddl)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"samba123@\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        else:
            # for some reason we get constraint violation instead of insufficient access error
            self.fail()

    def test_change_password2(self):
        """Make sure WP has no influence"""
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;WD)", "")
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;PS)", "")
        self.sd_utils.modify_sd_on_dn(self.get_user_dn(self.user_with_wp), sddl)
        mod = "(A;;WP;;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"samba123@\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        else:
            # for some reason we get constraint violation instead of insufficient access error
            self.fail()

    def test_change_password3(self):
        """Make sure WP has no influence"""
        mod = "(D;;WP;;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"samba123@\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")

    def test_change_password5(self):
        """Make sure rights have no influence on dBCSPwd"""
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;WD)", "")
        sddl = sddl.replace("(OA;;CR;ab721a53-1e2f-11d0-9819-00aa0040529b;;PS)", "")
        self.sd_utils.modify_sd_on_dn(self.get_user_dn(self.user_with_wp), sddl)
        mod = "(D;;WP;;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: dBCSPwd
dBCSPwd: XXXXXXXXXXXXXXXX
add: dBCSPwd
dBCSPwd: YYYYYYYYYYYYYYYY
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        else:
            self.fail()

    def test_change_password6(self):
        """Test uneven delete/adds"""
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()
        mod = "(OA;;CR;00299570-246d-11d0-a768-00aa006e0529;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
            # This fails on Windows 2000 domain level with constraint violation
        except LdbError, (num, _):
            self.assertTrue(num == ERR_CONSTRAINT_VIOLATION or
                            num == ERR_UNWILLING_TO_PERFORM)
        else:
            self.fail()


    def test_change_password7(self):
        """Try a password change operation without any CARs given"""
        #users have change password by default - remove for negative testing
        desc = self.sd_utils.read_sd_on_dn(self.get_user_dn(self.user_with_wp))
        sddl = desc.as_sddl(self.domain_sid)
        self.sd_utils.modify_sd_on_dn(self.get_user_dn(self.user_with_wp), sddl)
        #first change our own password
        self.ldb_user2.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_pc) + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"samba123@\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
""")
        #then someone else's
        self.ldb_user2.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"samba123@\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")

    def test_reset_password1(self):
        """Try a user password reset operation (unicodePwd) before and after granting CAR"""
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()
        mod = "(OA;;CR;00299570-246d-11d0-a768-00aa006e0529;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
""")

    def test_reset_password2(self):
        """Try a user password reset operation (userPassword) before and after granting CAR"""
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()
        mod = "(OA;;CR;00299570-246d-11d0-a768-00aa006e0529;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
""")
            # This fails on Windows 2000 domain level with constraint violation
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

    def test_reset_password3(self):
        """Grant WP and see what happens (unicodePwd)"""
        mod = "(A;;WP;;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_reset_password4(self):
        """Grant WP and see what happens (userPassword)"""
        mod = "(A;;WP;;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
""")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
        else:
            self.fail()

    def test_reset_password5(self):
        """Explicitly deny WP but grant CAR (unicodePwd)"""
        mod = "(D;;WP;;;PS)(OA;;CR;00299570-246d-11d0-a768-00aa006e0529;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
""")

    def test_reset_password6(self):
        """Explicitly deny WP but grant CAR (userPassword)"""
        mod = "(D;;WP;;;PS)(OA;;CR;00299570-246d-11d0-a768-00aa006e0529;;PS)"
        self.sd_utils.dacl_add_ace(self.get_user_dn(self.user_with_wp), mod)
        try:
            self.ldb_user.modify_ldif("""
dn: """ + self.get_user_dn(self.user_with_wp) + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
""")
            # This fails on Windows 2000 domain level with constraint violation
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

class AclExtendedTests(AclTests):

    def setUp(self):
        super(AclExtendedTests, self).setUp()
        #regular user, will be the creator
        self.u1 = "ext_u1"
        #regular user
        self.u2 = "ext_u2"
        #admin user
        self.u3 = "ext_u3"
        self.ldb_admin.newuser(self.u1, self.user_pass)
        self.ldb_admin.newuser(self.u2, self.user_pass)
        self.ldb_admin.newuser(self.u3, self.user_pass)
        self.ldb_admin.add_remove_group_members("Domain Admins", self.u3,
                                                add_members_operation=True)
        self.ldb_user1 = self.get_ldb_connection(self.u1, self.user_pass)
        self.ldb_user2 = self.get_ldb_connection(self.u2, self.user_pass)
        self.ldb_user3 = self.get_ldb_connection(self.u3, self.user_pass)
        self.user_sid1 = self.sd_utils.get_object_sid(self.get_user_dn(self.u1))
        self.user_sid2 = self.sd_utils.get_object_sid(self.get_user_dn(self.u2))

    def tearDown(self):
        super(AclExtendedTests, self).tearDown()
        delete_force(self.ldb_admin, self.get_user_dn(self.u1))
        delete_force(self.ldb_admin, self.get_user_dn(self.u2))
        delete_force(self.ldb_admin, self.get_user_dn(self.u3))
        delete_force(self.ldb_admin, "CN=ext_group1,OU=ext_ou1," + self.base_dn)
        delete_force(self.ldb_admin, "ou=ext_ou1," + self.base_dn)

    def test_ntSecurityDescriptor(self):
        #create empty ou
        self.ldb_admin.create_ou("ou=ext_ou1," + self.base_dn)
        #give u1 Create children access
        mod = "(A;;CC;;;%s)" % str(self.user_sid1)
        self.sd_utils.dacl_add_ace("OU=ext_ou1," + self.base_dn, mod)
        mod = "(A;;LC;;;%s)" % str(self.user_sid2)
        self.sd_utils.dacl_add_ace("OU=ext_ou1," + self.base_dn, mod)
        #create a group under that, grant RP to u2
        self.ldb_user1.newgroup("ext_group1", groupou="OU=ext_ou1", grouptype=4)
        mod = "(A;;RP;;;%s)" % str(self.user_sid2)
        self.sd_utils.dacl_add_ace("CN=ext_group1,OU=ext_ou1," + self.base_dn, mod)
        #u2 must not read the descriptor
        res = self.ldb_user2.search("CN=ext_group1,OU=ext_ou1," + self.base_dn,
                                    SCOPE_BASE, None, ["nTSecurityDescriptor"])
        self.assertNotEqual(len(res), 0)
        self.assertFalse("nTSecurityDescriptor" in res[0].keys())
        #grant RC to u2 - still no access
        mod = "(A;;RC;;;%s)" % str(self.user_sid2)
        self.sd_utils.dacl_add_ace("CN=ext_group1,OU=ext_ou1," + self.base_dn, mod)
        res = self.ldb_user2.search("CN=ext_group1,OU=ext_ou1," + self.base_dn,
                                    SCOPE_BASE, None, ["nTSecurityDescriptor"])
        self.assertNotEqual(len(res), 0)
        self.assertFalse("nTSecurityDescriptor" in res[0].keys())
        #u3 is member of administrators group, should be able to read sd
        res = self.ldb_user3.search("CN=ext_group1,OU=ext_ou1," + self.base_dn,
                                    SCOPE_BASE, None, ["nTSecurityDescriptor"])
        self.assertEqual(len(res),1)
        self.assertTrue("nTSecurityDescriptor" in res[0].keys())


class AclSPNTests(AclTests):

    def setUp(self):
        super(AclSPNTests, self).setUp()
        self.dcname = "TESTSRV8"
        self.rodcname = "TESTRODC8"
        self.computername = "testcomp8"
        self.test_user = "spn_test_user8"
        self.computerdn = "CN=%s,CN=computers,%s" % (self.computername, self.base_dn)
        self.dc_dn = "CN=%s,OU=Domain Controllers,%s" % (self.dcname, self.base_dn)
        self.site = "Default-First-Site-Name"
        self.rodcctx = dc_join(server=host, creds=creds, lp=lp,
            site=self.site, netbios_name=self.rodcname, targetdir=None,
            domain=None)
        self.dcctx = dc_join(server=host, creds=creds, lp=lp, site=self.site,
                netbios_name=self.dcname, targetdir=None, domain=None)
        self.ldb_admin.newuser(self.test_user, self.user_pass)
        self.ldb_user1 = self.get_ldb_connection(self.test_user, self.user_pass)
        self.user_sid1 = self.sd_utils.get_object_sid(self.get_user_dn(self.test_user))
        self.create_computer(self.computername, self.dcctx.dnsdomain)
        self.create_rodc(self.rodcctx)
        self.create_dc(self.dcctx)

    def tearDown(self):
        super(AclSPNTests, self).tearDown()
        self.rodcctx.cleanup_old_join()
        self.dcctx.cleanup_old_join()
        delete_force(self.ldb_admin, "cn=%s,cn=computers,%s" % (self.computername, self.base_dn))
        delete_force(self.ldb_admin, self.get_user_dn(self.test_user))

    def replace_spn(self, _ldb, dn, spn):
        print "Setting spn %s on %s" % (spn, dn)
        res = self.ldb_admin.search(dn, expression="(objectClass=*)",
                                    scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        if "servicePrincipalName" in res[0].keys():
            flag = FLAG_MOD_REPLACE
        else:
            flag = FLAG_MOD_ADD

        msg = Message()
        msg.dn = Dn(self.ldb_admin, dn)
        msg["servicePrincipalName"] = MessageElement(spn, flag,
                                                         "servicePrincipalName")
        _ldb.modify(msg)

    def create_computer(self, computername, domainname):
        dn = "CN=%s,CN=computers,%s" % (computername, self.base_dn)
        samaccountname = computername + "$"
        dnshostname = "%s.%s" % (computername, domainname)
        self.ldb_admin.add({
            "dn": dn,
            "objectclass": "computer",
            "sAMAccountName": samaccountname,
            "userAccountControl": str(samba.dsdb.UF_WORKSTATION_TRUST_ACCOUNT),
            "dNSHostName": dnshostname})

    # same as for join_RODC, but do not set any SPNs
    def create_rodc(self, ctx):
         ctx.krbtgt_dn = "CN=krbtgt_%s,CN=Users,%s" % (ctx.myname, ctx.base_dn)

         ctx.never_reveal_sid = [ "<SID=%s-%s>" % (ctx.domsid, security.DOMAIN_RID_RODC_DENY),
                                  "<SID=%s>" % security.SID_BUILTIN_ADMINISTRATORS,
                                  "<SID=%s>" % security.SID_BUILTIN_SERVER_OPERATORS,
                                  "<SID=%s>" % security.SID_BUILTIN_BACKUP_OPERATORS,
                                  "<SID=%s>" % security.SID_BUILTIN_ACCOUNT_OPERATORS ]
         ctx.reveal_sid = "<SID=%s-%s>" % (ctx.domsid, security.DOMAIN_RID_RODC_ALLOW)

         mysid = ctx.get_mysid()
         admin_dn = "<SID=%s>" % mysid
         ctx.managedby = admin_dn

         ctx.userAccountControl = (samba.dsdb.UF_WORKSTATION_TRUST_ACCOUNT |
                              samba.dsdb.UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION |
                              samba.dsdb.UF_PARTIAL_SECRETS_ACCOUNT)

         ctx.connection_dn = "CN=RODC Connection (FRS),%s" % ctx.ntds_dn
         ctx.secure_channel_type = misc.SEC_CHAN_RODC
         ctx.RODC = True
         ctx.replica_flags  =  (drsuapi.DRSUAPI_DRS_INIT_SYNC |
                                drsuapi.DRSUAPI_DRS_PER_SYNC |
                                drsuapi.DRSUAPI_DRS_GET_ANC |
                                drsuapi.DRSUAPI_DRS_NEVER_SYNCED |
                                drsuapi.DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING)

         ctx.join_add_objects()

    def create_dc(self, ctx):
        ctx.userAccountControl = samba.dsdb.UF_SERVER_TRUST_ACCOUNT | samba.dsdb.UF_TRUSTED_FOR_DELEGATION
        ctx.secure_channel_type = misc.SEC_CHAN_BDC
        ctx.replica_flags = (drsuapi.DRSUAPI_DRS_WRIT_REP |
                             drsuapi.DRSUAPI_DRS_INIT_SYNC |
                             drsuapi.DRSUAPI_DRS_PER_SYNC |
                             drsuapi.DRSUAPI_DRS_FULL_SYNC_IN_PROGRESS |
                             drsuapi.DRSUAPI_DRS_NEVER_SYNCED)

        ctx.join_add_objects()

    def dc_spn_test(self, ctx):
        netbiosdomain = self.dcctx.get_domain_name()
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s/%s" % (ctx.myname, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        mod = "(OA;;SW;f3a64788-5306-11d1-a9c5-0000f80367c1;;%s)" % str(self.user_sid1)
        self.sd_utils.dacl_add_ace(ctx.acct_dn, mod)
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s/%s" % (ctx.myname, netbiosdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s" % (ctx.myname))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s.%s/%s" %
                         (ctx.myname, ctx.dnsdomain, netbiosdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s/%s" % (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "HOST/%s.%s/%s" %
                         (ctx.myname, ctx.dnsdomain, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "GC/%s.%s/%s" %
                         (ctx.myname, ctx.dnsdomain, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s/%s" % (ctx.myname, netbiosdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s.%s/%s" %
                         (ctx.myname, ctx.dnsdomain, netbiosdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s" % (ctx.myname))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s/%s" % (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s.%s/%s" %
                         (ctx.myname, ctx.dnsdomain, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "DNS/%s/%s" % (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "RestrictedKrbHost/%s/%s" %
                         (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "RestrictedKrbHost/%s" %
                         (ctx.myname))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "Dfsr-12F9A27C-BF97-4787-9364-D31B6C55EB04/%s/%s" %
                         (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "NtFrs-88f5d2bd-b646-11d2-a6d3-00c04fc9b232/%s/%s" %
                         (ctx.myname, ctx.dnsdomain))
        self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s._msdcs.%s" %
                         (ctx.ntds_guid, ctx.dnsdomain))

        #the following spns do not match the restrictions and should fail
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s.%s/ForestDnsZones.%s" %
                             (ctx.myname, ctx.dnsdomain, ctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "ldap/%s.%s/DomainDnsZones.%s" %
                             (ctx.myname, ctx.dnsdomain, ctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "nosuchservice/%s/%s" % ("abcd", "abcd"))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "GC/%s.%s/%s" %
                             (ctx.myname, ctx.dnsdomain, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, ctx.acct_dn, "E3514235-4B06-11D1-AB04-00C04FC2DCD2/%s/%s" %
                             (ctx.ntds_guid, ctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

    def test_computer_spn(self):
        # with WP, any value can be set
        netbiosdomain = self.dcctx.get_domain_name()
        self.replace_spn(self.ldb_admin, self.computerdn, "HOST/%s/%s" %
                         (self.computername, netbiosdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "HOST/%s" % (self.computername))
        self.replace_spn(self.ldb_admin, self.computerdn, "HOST/%s.%s/%s" %
                         (self.computername, self.dcctx.dnsdomain, netbiosdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "HOST/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "HOST/%s.%s/%s" %
                         (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "GC/%s.%s/%s" %
                         (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s/%s" % (self.computername, netbiosdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s.%s/ForestDnsZones.%s" %
                         (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s.%s/DomainDnsZones.%s" %
                         (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s.%s/%s" %
                         (self.computername, self.dcctx.dnsdomain, netbiosdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s" % (self.computername))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "ldap/%s.%s/%s" %
                         (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "DNS/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "RestrictedKrbHost/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "RestrictedKrbHost/%s" %
                         (self.computername))
        self.replace_spn(self.ldb_admin, self.computerdn, "Dfsr-12F9A27C-BF97-4787-9364-D31B6C55EB04/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "NtFrs-88f5d2bd-b646-11d2-a6d3-00c04fc9b232/%s/%s" %
                         (self.computername, self.dcctx.dnsdomain))
        self.replace_spn(self.ldb_admin, self.computerdn, "nosuchservice/%s/%s" % ("abcd", "abcd"))

        #user has neither WP nor Validated-SPN, access denied expected
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s/%s" % (self.computername, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        mod = "(OA;;SW;f3a64788-5306-11d1-a9c5-0000f80367c1;;%s)" % str(self.user_sid1)
        self.sd_utils.dacl_add_ace(self.computerdn, mod)
        #grant Validated-SPN and check which values are accepted
        #see 3.1.1.5.3.1.1.4 servicePrincipalName for reference

        # for regular computer objects we shouldalways get constraint violation

        # This does not pass against Windows, although it should according to docs
        self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s" % (self.computername))
        self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s.%s" %
                             (self.computername, self.dcctx.dnsdomain))

        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s/%s" % (self.computername, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s.%s/%s" %
                             (self.computername, self.dcctx.dnsdomain, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s/%s" %
                             (self.computername, self.dcctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "HOST/%s.%s/%s" %
                             (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "GC/%s.%s/%s" %
                             (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "ldap/%s/%s" % (self.computername, netbiosdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        try:
            self.replace_spn(self.ldb_user1, self.computerdn, "ldap/%s.%s/ForestDnsZones.%s" %
                             (self.computername, self.dcctx.dnsdomain, self.dcctx.dnsdomain))
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

    def test_spn_rwdc(self):
        self.dc_spn_test(self.dcctx)

    def test_spn_rodc(self):
        self.dc_spn_test(self.rodcctx)


# Important unit running information

ldb = SamDB(ldaphost, credentials=creds, session_info=system_session(lp), lp=lp)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(AclAddTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(AclModifyTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(AclDeleteTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(AclRenameTests)).wasSuccessful():
    rc = 1

# Get the old "dSHeuristics" if it was set
dsheuristics = ldb.get_dsheuristics()
# Set the "dSHeuristics" to activate the correct "userPassword" behaviour
ldb.set_dsheuristics("000000001")
# Get the old "minPwdAge"
minPwdAge = ldb.get_minPwdAge()
# Set it temporarely to "0"
ldb.set_minPwdAge("0")
if not runner.run(unittest.makeSuite(AclCARTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(AclSearchTests)).wasSuccessful():
    rc = 1
# Reset the "dSHeuristics" as they were before
ldb.set_dsheuristics(dsheuristics)
# Reset the "minPwdAge" as it was before
ldb.set_minPwdAge(minPwdAge)

if not runner.run(unittest.makeSuite(AclExtendedTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(AclSPNTests)).wasSuccessful():
    rc = 1
sys.exit(rc)
