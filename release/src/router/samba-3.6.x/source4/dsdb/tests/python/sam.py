#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This is a port of the original in testprogs/ejs/ldap.js

import optparse
import sys
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT, ERR_ATTRIBUTE_OR_VALUE_EXISTS
from ldb import ERR_ENTRY_ALREADY_EXISTS, ERR_UNWILLING_TO_PERFORM
from ldb import ERR_OTHER, ERR_NO_SUCH_ATTRIBUTE
from ldb import ERR_OBJECT_CLASS_VIOLATION
from ldb import ERR_CONSTRAINT_VIOLATION
from ldb import ERR_UNDEFINED_ATTRIBUTE_TYPE
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_ADD, FLAG_MOD_REPLACE, FLAG_MOD_DELETE
from samba.samdb import SamDB
from samba.dsdb import (UF_NORMAL_ACCOUNT, UF_ACCOUNTDISABLE,
    UF_WORKSTATION_TRUST_ACCOUNT, UF_SERVER_TRUST_ACCOUNT,
    UF_PARTIAL_SECRETS_ACCOUNT, UF_TEMP_DUPLICATE_ACCOUNT,
    UF_PASSWD_NOTREQD, ATYPE_NORMAL_ACCOUNT,
    GTYPE_SECURITY_BUILTIN_LOCAL_GROUP, GTYPE_SECURITY_DOMAIN_LOCAL_GROUP,
    GTYPE_SECURITY_GLOBAL_GROUP, GTYPE_SECURITY_UNIVERSAL_GROUP,
    GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP, GTYPE_DISTRIBUTION_GLOBAL_GROUP,
    GTYPE_DISTRIBUTION_UNIVERSAL_GROUP,
    ATYPE_SECURITY_GLOBAL_GROUP, ATYPE_SECURITY_UNIVERSAL_GROUP,
    ATYPE_SECURITY_LOCAL_GROUP, ATYPE_DISTRIBUTION_GLOBAL_GROUP,
    ATYPE_DISTRIBUTION_UNIVERSAL_GROUP, ATYPE_DISTRIBUTION_LOCAL_GROUP,
    ATYPE_WORKSTATION_TRUST)
from samba.dcerpc.security import (DOMAIN_RID_USERS, DOMAIN_RID_DOMAIN_MEMBERS,
    DOMAIN_RID_DCS, DOMAIN_RID_READONLY_DCS)

from subunit.run import SubunitTestRunner
import unittest

from samba.dcerpc import security
from samba.tests import delete_force

parser = optparse.OptionParser("sam.py [options] <host>")
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

lp = sambaopts.get_loadparm()
creds = credopts.get_credentials(lp)

class SamTests(unittest.TestCase):

    def setUp(self):
        super(SamTests, self).setUp()
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()

        print "baseDN: %s\n" % self.base_dn

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptest\,specialuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)

    def test_users_groups(self):
        """This tests the SAM users and groups behaviour"""
        print "Testing users and groups behaviour\n"

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        ldb.add({
            "dn": "cn=ldaptestgroup2,cn=users," + self.base_dn,
            "objectclass": "group"})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["objectSID"])
        self.assertTrue(len(res1) == 1)
        group_rid_1 = security.dom_sid(ldb.schema_format_value("objectSID",
          res1[0]["objectSID"][0])).split()[1]

        res1 = ldb.search("cn=ldaptestgroup2,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["objectSID"])
        self.assertTrue(len(res1) == 1)
        group_rid_2 = security.dom_sid(ldb.schema_format_value("objectSID",
          res1[0]["objectSID"][0])).split()[1]

        # Try to create a user with an invalid account name
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "sAMAccountName": "administrator"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Try to create a user with an invalid account name
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "sAMAccountName": []})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Try to create a user with an invalid primary group
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "primaryGroupID": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Try to Create a user with a valid primary group
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "primaryGroupID": str(group_rid_1)})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Test to see how we should behave when the user account doesn't
        # exist
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement("0", FLAG_MOD_REPLACE,
          "primaryGroupID")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # Test to see how we should behave when the account isn't a user
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement("0", FLAG_MOD_REPLACE,
          "primaryGroupID")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # Test default primary groups on add operations

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_USERS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user",
            "userAccountControl": str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD) })

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_USERS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # unfortunately the INTERDOMAIN_TRUST_ACCOUNT case cannot be tested
        # since such accounts aren't directly creatable (ACCESS_DENIED)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "computer",
            "userAccountControl": str(UF_WORKSTATION_TRUST_ACCOUNT | UF_PASSWD_NOTREQD) })

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_DOMAIN_MEMBERS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "computer",
            "userAccountControl": str(UF_SERVER_TRUST_ACCOUNT | UF_PASSWD_NOTREQD) })

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_DCS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Read-only DC accounts are only creatable by
        # UF_WORKSTATION_TRUST_ACCOUNT and work only on DCs >= 2008 (therefore
        # we have a fallback in the assertion)
        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "computer",
            "userAccountControl": str(UF_PARTIAL_SECRETS_ACCOUNT | UF_WORKSTATION_TRUST_ACCOUNT | UF_PASSWD_NOTREQD) })

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertTrue(res1[0]["primaryGroupID"][0] == str(DOMAIN_RID_READONLY_DCS) or
                        res1[0]["primaryGroupID"][0] == str(DOMAIN_RID_DOMAIN_MEMBERS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Test default primary groups on modify operations

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD), FLAG_MOD_REPLACE,
          "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_USERS))

        # unfortunately the INTERDOMAIN_TRUST_ACCOUNT case cannot be tested
        # since such accounts aren't directly creatable (ACCESS_DENIED)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "computer"})

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_USERS))

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(UF_WORKSTATION_TRUST_ACCOUNT | UF_PASSWD_NOTREQD), FLAG_MOD_REPLACE,
          "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_DOMAIN_MEMBERS))

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(UF_SERVER_TRUST_ACCOUNT | UF_PASSWD_NOTREQD), FLAG_MOD_REPLACE,
          "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(res1[0]["primaryGroupID"][0], str(DOMAIN_RID_DCS))

        # Read-only DC accounts are only creatable by
        # UF_WORKSTATION_TRUST_ACCOUNT and work only on DCs >= 2008 (therefore
        # we have a fallback in the assertion)
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(UF_PARTIAL_SECRETS_ACCOUNT | UF_WORKSTATION_TRUST_ACCOUNT | UF_PASSWD_NOTREQD), FLAG_MOD_REPLACE,
          "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupID"])
        self.assertTrue(len(res1) == 1)
        self.assertTrue(res1[0]["primaryGroupID"][0] == str(DOMAIN_RID_READONLY_DCS) or
                        res1[0]["primaryGroupID"][0] == str(DOMAIN_RID_DOMAIN_MEMBERS))

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Recreate account for further tests

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        # Try to set an invalid account name
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement("administrator", FLAG_MOD_REPLACE,
          "sAMAccountName")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # But to reset the actual "sAMAccountName" should still be possible
        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountName"])
        self.assertTrue(len(res1) == 1)
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement(res1[0]["sAMAccountName"][0], FLAG_MOD_REPLACE,
          "sAMAccountName")
        ldb.modify(m)

        # And another (free) name should be possible as well
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement("xxx_ldaptestuser_xxx", FLAG_MOD_REPLACE,
          "sAMAccountName")
        ldb.modify(m)

        # We should be able to reset our actual primary group
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement(str(DOMAIN_RID_USERS), FLAG_MOD_REPLACE,
          "primaryGroupID")
        ldb.modify(m)

        # Try to add invalid primary group
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement("0", FLAG_MOD_REPLACE,
          "primaryGroupID")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Try to make group 1 primary - should be denied since it is not yet
        # secondary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement(str(group_rid_1),
          FLAG_MOD_REPLACE, "primaryGroupID")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Make group 1 secondary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_REPLACE, "member")
        ldb.modify(m)

        # Make group 1 primary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement(str(group_rid_1),
          FLAG_MOD_REPLACE, "primaryGroupID")
        ldb.modify(m)

        # Try to delete group 1 - should be denied
        try:
            ldb.delete("cn=ldaptestgroup,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # Try to add group 1 also as secondary - should be denied
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # Try to add invalid member to group 1 - should be denied
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement(
          "cn=ldaptestuser3,cn=users," + self.base_dn,
          FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # Make group 2 secondary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_ADD, "member")
        ldb.modify(m)

        # Swap the groups
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement(str(group_rid_2),
          FLAG_MOD_REPLACE, "primaryGroupID")
        ldb.modify(m)

        # Swap the groups (does not really make sense but does the same)
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement(str(group_rid_1),
          FLAG_MOD_REPLACE, "primaryGroupID")
        m["primaryGroupID"] = MessageElement(str(group_rid_2),
          FLAG_MOD_REPLACE, "primaryGroupID")
        ldb.modify(m)

        # Old primary group should contain a "member" attribute for the user,
        # the new shouldn't contain anymore one
        res1 = ldb.search("cn=ldaptestgroup, cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["member"])
        self.assertTrue(len(res1) == 1)
        self.assertTrue(len(res1[0]["member"]) == 1)
        self.assertEquals(res1[0]["member"][0].lower(),
          ("cn=ldaptestuser,cn=users," + self.base_dn).lower())

        res1 = ldb.search("cn=ldaptestgroup2, cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["member"])
        self.assertTrue(len(res1) == 1)
        self.assertFalse("member" in res1[0])

        # Primary group member
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_DELETE, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Delete invalid group member
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser1,cn=users," + self.base_dn,
                                     FLAG_MOD_DELETE, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Also this should be denied
        try:
            ldb.add({
              "dn": "cn=ldaptestuser2,cn=users," + self.base_dn,
              "objectclass": "user",
              "primaryGroupID": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Recreate user accounts

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        ldb.add({
            "dn": "cn=ldaptestuser2,cn=users," + self.base_dn,
            "objectclass": "user"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_ADD, "member")
        ldb.modify(m)

        # Already added
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # Already added, but as <SID=...>
        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["objectSid"])
        self.assertTrue(len(res1) == 1)
        sid_bin = res1[0]["objectSid"][0]
        sid_str = ("<SID=" + ldb.schema_format_value("objectSid", sid_bin) + ">").upper()

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement(sid_str, FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # Invalid member
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser1,cn=users," + self.base_dn,
                                     FLAG_MOD_REPLACE, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # Invalid member
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement(["cn=ldaptestuser,cn=users," + self.base_dn,
                                      "cn=ldaptestuser1,cn=users," + self.base_dn],
                                     FLAG_MOD_REPLACE, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # Invalid member
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
                                     FLAG_MOD_REPLACE, "member")
        m["member"] = MessageElement("cn=ldaptestuser1,cn=users," + self.base_dn,
                                     FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = MessageElement(["cn=ldaptestuser,cn=users," + self.base_dn,
                                      "cn=ldaptestuser2,cn=users," + self.base_dn],
                                     FLAG_MOD_REPLACE, "member")
        ldb.modify(m)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)

        # Make also a small test for accounts with special DNs ("," in this case)
        ldb.add({
            "dn": "cn=ldaptest\,specialuser,cn=users," + self.base_dn,
            "objectclass": "user"})
        delete_force(self.ldb, "cn=ldaptest\,specialuser,cn=users," + self.base_dn)

    def test_sam_attributes(self):
        """Test the behaviour of special attributes of SAM objects"""
        print "Testing the behaviour of special attributes of SAM objects\n"""

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})
        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(str(GTYPE_SECURITY_GLOBAL_GROUP), FLAG_MOD_ADD,
          "groupType")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        # Delete protection tests

        for attr in ["nTSecurityDescriptor", "objectSid", "sAMAccountType",
                     "sAMAccountName", "groupType"]:

            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m[attr] = MessageElement([], FLAG_MOD_REPLACE, attr)
            try:
                ldb.modify(m)
                self.fail()
            except LdbError, (num, _):
                self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m[attr] = MessageElement([], FLAG_MOD_DELETE, attr)
            try:
                ldb.modify(m)
                self.fail()
            except LdbError, (num, _):
                self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["primaryGroupID"] = MessageElement("513", FLAG_MOD_ADD,
          "primaryGroupID")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD), FLAG_MOD_ADD,
          "userAccountControl")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectSid"] = MessageElement("xxxxxxxxxxxxxxxx", FLAG_MOD_ADD,
          "objectSid")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sAMAccountType"] = MessageElement("0", FLAG_MOD_ADD,
          "sAMAccountType")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement("test", FLAG_MOD_ADD,
          "sAMAccountName")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        # Delete protection tests

        for attr in ["nTSecurityDescriptor", "objectSid", "sAMAccountType",
                     "sAMAccountName", "primaryGroupID", "userAccountControl",
                     "accountExpires", "badPasswordTime", "badPwdCount",
                     "codePage", "countryCode", "lastLogoff", "lastLogon",
                     "logonCount", "pwdLastSet"]:

            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
            m[attr] = MessageElement([], FLAG_MOD_REPLACE, attr)
            try:
                ldb.modify(m)
                self.fail()
            except LdbError, (num, _):
                self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
            m[attr] = MessageElement([], FLAG_MOD_DELETE, attr)
            try:
                ldb.modify(m)
                self.fail()
            except LdbError, (num, _):
                self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_primary_group_token_constructed(self):
        """Test the primary group token behaviour (hidden-generated-readonly attribute on groups) and some other constructed attributes"""
        print "Testing primary group token behaviour and other constructed attributes\n"

        try:
            ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "primaryGroupToken": "100"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNDEFINED_ATTRIBUTE_TYPE)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        # Testing for one invalid, and one valid operational attribute, but also the things they are built from
        res1 = ldb.search(self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupToken", "canonicalName", "objectClass", "objectSid"])
        self.assertTrue(len(res1) == 1)
        self.assertFalse("primaryGroupToken" in res1[0])
        self.assertTrue("canonicalName" in res1[0])
        self.assertTrue("objectClass" in res1[0])
        self.assertTrue("objectSid" in res1[0])

        res1 = ldb.search(self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupToken", "canonicalName"])
        self.assertTrue(len(res1) == 1)
        self.assertFalse("primaryGroupToken" in res1[0])
        self.assertFalse("objectSid" in res1[0])
        self.assertFalse("objectClass" in res1[0])
        self.assertTrue("canonicalName" in res1[0])

        res1 = ldb.search("cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupToken"])
        self.assertTrue(len(res1) == 1)
        self.assertFalse("primaryGroupToken" in res1[0])

        res1 = ldb.search("cn=ldaptestuser, cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupToken"])
        self.assertTrue(len(res1) == 1)
        self.assertFalse("primaryGroupToken" in res1[0])

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE)
        self.assertTrue(len(res1) == 1)
        self.assertFalse("primaryGroupToken" in res1[0])

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["primaryGroupToken", "objectSID"])
        self.assertTrue(len(res1) == 1)
        primary_group_token = int(res1[0]["primaryGroupToken"][0])

        rid = security.dom_sid(ldb.schema_format_value("objectSID", res1[0]["objectSID"][0])).split()[1]
        self.assertEquals(primary_group_token, rid)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["primaryGroupToken"] = "100"
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_tokenGroups(self):
        """Test the tokenGroups behaviour (hidden-generated-readonly attribute on SAM objects)"""
        print "Testing tokenGroups behaviour\n"

        # The domain object shouldn't contain any "tokenGroups" entry
        res = ldb.search(self.base_dn, scope=SCOPE_BASE, attrs=["tokenGroups"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("tokenGroups" in res[0])

        # The domain administrator should contain "tokenGroups" entries
        # (the exact number depends on the domain/forest function level and the
        # DC software versions)
        res = ldb.search("cn=Administrator,cn=Users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["tokenGroups"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("tokenGroups" in res[0])

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        # This testuser should contain at least two "tokenGroups" entries
        # (exactly two on an unmodified "Domain Users" and "Users" group)
        res = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["tokenGroups"])
        self.assertTrue(len(res) == 1)
        self.assertTrue(len(res[0]["tokenGroups"]) >= 2)

        # one entry which we need to find should point to domains "Domain Users"
        # group and another entry should point to the builtin "Users"group
        domain_users_group_found = False
        users_group_found = False
        for sid in res[0]["tokenGroups"]:
            rid = security.dom_sid(ldb.schema_format_value("objectSID", sid)).split()[1]
            if rid == 513:
                domain_users_group_found = True
            if rid == 545:
                users_group_found = True

        self.assertTrue(domain_users_group_found)
        self.assertTrue(users_group_found)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

    def test_groupType(self):
        """Test the groupType behaviour"""
        print "Testing groupType behaviour\n"

        # You can never create or change to a
        # "GTYPE_SECURITY_BUILTIN_LOCAL_GROUP"

        # Add operation

        # Invalid attribute
        try:
            ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "groupType": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        try:
            ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "groupType": str(GTYPE_SECURITY_BUILTIN_LOCAL_GROUP)})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_SECURITY_GLOBAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_SECURITY_UNIVERSAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_SECURITY_DOMAIN_LOCAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_LOCAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_DISTRIBUTION_GLOBAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_GLOBAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_UNIVERSAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "groupType": str(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP)})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_LOCAL_GROUP)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        # Modify operation

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        # We can change in this direction: global <-> universal <-> local
        # On each step also the group type itself (security/distribution) is
        # variable.

        # After creation we should have a "security global group"
        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Invalid attribute
        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement("0",
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Security groups

        # Default is "global group"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Change to "local" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_SECURITY_DOMAIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_SECURITY_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Change back to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_SECURITY_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)

        # Change to "local"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_DOMAIN_LOCAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_LOCAL_GROUP)

        # Change to "global" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_SECURITY_GLOBAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change to "builtin local" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_SECURITY_BUILTIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        # Change back to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_SECURITY_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)

        # Change to "builtin local" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_SECURITY_BUILTIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Change to "builtin local" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_SECURITY_BUILTIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Distribution groups

        # Default is "global group"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_DISTRIBUTION_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_GLOBAL_GROUP)

        # Change to local (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_UNIVERSAL_GROUP)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_DISTRIBUTION_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_GLOBAL_GROUP)

        # Change back to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_UNIVERSAL_GROUP)

        # Change to "local"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_LOCAL_GROUP)

        # Change to "global" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_DISTRIBUTION_GLOBAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change back to "universal"

        # Try to add invalid member to group 1 - should be denied
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement(
          "cn=ldaptestuser3,cn=users," + self.base_dn,
          FLAG_MOD_ADD, "member")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # Make group 2 secondary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_UNIVERSAL_GROUP)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_DISTRIBUTION_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_GLOBAL_GROUP)

        # Both group types: this performs only random checks - all possibilities
        # would require too much code.

        # Default is "global group"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Change to "local" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_UNIVERSAL_GROUP)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        # Change back to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_SECURITY_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)

        # Change to "local"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_DISTRIBUTION_LOCAL_GROUP)

        # Change to "global" (shouldn't work)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
            m["groupType"] = MessageElement(
              str(GTYPE_DISTRIBUTION_GLOBAL_GROUP),
              FLAG_MOD_REPLACE, "groupType")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Change back to "universal"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
         str(GTYPE_SECURITY_UNIVERSAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_UNIVERSAL_GROUP)

        # Change back to "global"

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["groupType"] = MessageElement(
          str(GTYPE_SECURITY_GLOBAL_GROUP),
          FLAG_MOD_REPLACE, "groupType")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_SECURITY_GLOBAL_GROUP)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_userAccountControl(self):
        """Test the userAccountControl behaviour"""
        print "Testing userAccountControl behaviour\n"

        # With a user object

        # Add operation

        # As user you can only set a normal account.
        # The UF_PASSWD_NOTREQD flag is needed since we haven't requested a
        # password yet.
        # With SYSTEM rights you can set a interdomain trust account.

        # Invalid attribute
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "userAccountControl": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

# This has to wait until s4 supports it (needs a password module change)
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
#                "objectclass": "user",
#                "userAccountControl": str(UF_NORMAL_ACCOUNT)})
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
#        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user",
            "userAccountControl": str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD)})

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE == 0)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "userAccountControl": str(UF_TEMP_DUPLICATE_ACCOUNT)})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

# This isn't supported yet in s4
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
#                "objectclass": "user",
#                "userAccountControl": str(UF_SERVER_TRUST_ACCOUNT)})
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)
#        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
#
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
#                "objectclass": "user",
#                "userAccountControl": str(UF_WORKSTATION_TRUST_ACCOUNT)})
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)
#        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

# This isn't supported yet in s4 - needs ACL module adaption
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
#                "objectclass": "user",
#                "userAccountControl": str(UF_INTERDOMAIN_TRUST_ACCOUNT)})
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
#        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Modify operation

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        # After creation we should have a normal account
        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE != 0)

        # As user you can only switch from a normal account to a workstation
        # trust account and back.
        # The UF_PASSWD_NOTREQD flag is needed since we haven't requested a
        # password yet.
        # With SYSTEM rights you can switch to a interdomain trust account.

        # Invalid attribute
        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
            m["userAccountControl"] = MessageElement("0",
              FLAG_MOD_REPLACE, "userAccountControl")
            ldb.modify(m)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

# This has to wait until s4 supports it (needs a password module change)
#        try:
#            m = Message()
#            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
#            m["userAccountControl"] = MessageElement(
#              str(UF_NORMAL_ACCOUNT),
#              FLAG_MOD_REPLACE, "userAccountControl")
#            ldb.modify(m)
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE == 0)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
            m["userAccountControl"] = MessageElement(
              str(UF_TEMP_DUPLICATE_ACCOUNT),
              FLAG_MOD_REPLACE, "userAccountControl")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)

# This isn't supported yet in s4
#        try:
#            m = Message()
#            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
#            m["userAccountControl"] = MessageElement(
#              str(UF_SERVER_TRUST_ACCOUNT),
#              FLAG_MOD_REPLACE, "userAccountControl")
#            ldb.modify(m)
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_WORKSTATION_TRUST_ACCOUNT),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)

# This isn't supported yet in s4 - needs ACL module adaption
#        try:
#            m = Message()
#            m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
#            m["userAccountControl"] = MessageElement(
#              str(UF_INTERDOMAIN_TRUST_ACCOUNT),
#              FLAG_MOD_REPLACE, "userAccountControl")
#            ldb.modify(m)
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        # With a computer object

        # Add operation

        # As computer you can set a normal account and a server trust account.
        # The UF_PASSWD_NOTREQD flag is needed since we haven't requested a
        # password yet.
        # With SYSTEM rights you can set a interdomain trust account.

        # Invalid attribute
        try:
            ldb.add({
                "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
                "objectclass": "computer",
                "userAccountControl": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

# This has to wait until s4 supports it (needs a password module change)
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
#                "objectclass": "computer",
#                "userAccountControl": str(UF_NORMAL_ACCOUNT)})
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
#        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "userAccountControl": str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD)})

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE == 0)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        try:
            ldb.add({
                "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
                "objectclass": "computer",
                "userAccountControl": str(UF_TEMP_DUPLICATE_ACCOUNT)})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "userAccountControl": str(UF_SERVER_TRUST_ACCOUNT)})

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        try:
            ldb.add({
                "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
                "objectclass": "computer",
                "userAccountControl": str(UF_WORKSTATION_TRUST_ACCOUNT)})
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

# This isn't supported yet in s4 - needs ACL module adaption
#        try:
#            ldb.add({
#                "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
#                "objectclass": "computer",
#                "userAccountControl": str(UF_INTERDOMAIN_TRUST_ACCOUNT)})
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)
#        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        # Modify operation

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer"})

        # After creation we should have a normal account
        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE != 0)

        # As computer you can switch from a normal account to a workstation
        # or server trust account and back (also swapping between trust
        # accounts is allowed).
        # The UF_PASSWD_NOTREQD flag is needed since we haven't requested a
        # password yet.
        # With SYSTEM rights you can switch to a interdomain trust account.

        # Invalid attribute
        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
            m["userAccountControl"] = MessageElement("0",
              FLAG_MOD_REPLACE, "userAccountControl")
            ldb.modify(m)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

# This has to wait until s4 supports it (needs a password module change)
#        try:
#            m = Message()
#            m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
#            m["userAccountControl"] = MessageElement(
#              str(UF_NORMAL_ACCOUNT),
#              FLAG_MOD_REPLACE, "userAccountControl")
#            ldb.modify(m)
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["sAMAccountType", "userAccountControl"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)
        self.assertTrue(int(res1[0]["userAccountControl"][0]) & UF_ACCOUNTDISABLE == 0)

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
            m["userAccountControl"] = MessageElement(
              str(UF_TEMP_DUPLICATE_ACCOUNT),
              FLAG_MOD_REPLACE, "userAccountControl")
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_SERVER_TRUST_ACCOUNT),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_WORKSTATION_TRUST_ACCOUNT),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_NORMAL_ACCOUNT)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_SERVER_TRUST_ACCOUNT),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["userAccountControl"] = MessageElement(
          str(UF_WORKSTATION_TRUST_ACCOUNT),
          FLAG_MOD_REPLACE, "userAccountControl")
        ldb.modify(m)

        res1 = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["sAMAccountType"])
        self.assertTrue(len(res1) == 1)
        self.assertEquals(int(res1[0]["sAMAccountType"][0]),
          ATYPE_WORKSTATION_TRUST)

# This isn't supported yet in s4 - needs ACL module adaption
#        try:
#            m = Message()
#            m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
#            m["userAccountControl"] = MessageElement(
#              str(UF_INTERDOMAIN_TRUST_ACCOUNT),
#              FLAG_MOD_REPLACE, "userAccountControl")
#            ldb.modify(m)
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

    def test_service_principal_name_updates(self):
        """Test the servicePrincipalNames update behaviour"""
        print "Testing servicePrincipalNames update behaviour\n"

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "dNSHostName": "testname.testdom"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("servicePrincipalName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "servicePrincipalName": "HOST/testname.testdom"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["dNSHostName"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("dNSHostName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "dNSHostName": "testname2.testdom",
            "servicePrincipalName": "HOST/testname.testdom"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["dNSHostName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["dNSHostName"][0], "testname2.testdom")

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname.testdom")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname.testdoM",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname.testdom")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname2.testdom2",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2.testdom2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement([],
                                          FLAG_MOD_DELETE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2.testdom2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname.testdom3",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2.testdom2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname2.testdom2",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname3.testdom3",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        m["servicePrincipalName"] = MessageElement("HOST/testname2.testdom2",
                                                   FLAG_MOD_REPLACE,
                                                   "servicePrincipalName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname3.testdom3")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["servicePrincipalName"] = MessageElement("HOST/testname2.testdom2",
                                                   FLAG_MOD_REPLACE,
                                                   "servicePrincipalName")
        m["dNSHostName"] = MessageElement("testname4.testdom4",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2.testdom2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["servicePrincipalName"] = MessageElement([],
                                                   FLAG_MOD_DELETE,
                                                   "servicePrincipalName")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname2.testdom2",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("servicePrincipalName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "sAMAccountName": "testname$"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("servicePrincipalName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "servicePrincipalName": "HOST/testname"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["sAMAccountName"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("sAMAccountName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "sAMAccountName": "testname$",
            "servicePrincipalName": "HOST/testname"})

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["sAMAccountName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["sAMAccountName"][0], "testname$")

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testnamE$",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testname",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("test$name$",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/test$name")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testname2",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testname3",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        m["servicePrincipalName"] = MessageElement("HOST/testname2",
                                                   FLAG_MOD_REPLACE,
                                                   "servicePrincipalName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname3")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["servicePrincipalName"] = MessageElement("HOST/testname2",
                                                   FLAG_MOD_REPLACE,
                                                   "servicePrincipalName")
        m["sAMAccountName"] = MessageElement("testname4",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["servicePrincipalName"][0],
                          "HOST/testname2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["servicePrincipalName"] = MessageElement([],
                                                   FLAG_MOD_DELETE,
                                                   "servicePrincipalName")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testname2",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("servicePrincipalName" in res[0])

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "dNSHostName": "testname.testdom",
            "sAMAccountName": "testname$",
            "servicePrincipalName": [ "HOST/testname.testdom", "HOST/testname" ]
        })

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["dNSHostName"] = MessageElement("testname2.testdom",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        m["sAMAccountName"] = MessageElement("testname2$",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["dNSHostName", "sAMAccountName", "servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["dNSHostName"][0], "testname2.testdom")
        self.assertEquals(res[0]["sAMAccountName"][0], "testname2$")
        self.assertTrue(res[0]["servicePrincipalName"][0] == "HOST/testname2" or
                        res[0]["servicePrincipalName"][1] == "HOST/testname2")
        self.assertTrue(res[0]["servicePrincipalName"][0] == "HOST/testname2.testdom" or
                        res[0]["servicePrincipalName"][1] == "HOST/testname2.testdom")

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "dNSHostName": "testname.testdom",
            "sAMAccountName": "testname$",
            "servicePrincipalName": [ "HOST/testname.testdom", "HOST/testname" ]
        })

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testname2$",
                                             FLAG_MOD_REPLACE, "sAMAccountName")
        m["dNSHostName"] = MessageElement("testname2.testdom",
                                          FLAG_MOD_REPLACE, "dNSHostName")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestcomputer,cn=computers," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["dNSHostName", "sAMAccountName", "servicePrincipalName"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["dNSHostName"][0], "testname2.testdom")
        self.assertEquals(res[0]["sAMAccountName"][0], "testname2$")
        self.assertTrue(res[0]["servicePrincipalName"][0] == "HOST/testname2" or
                        res[0]["servicePrincipalName"][1] == "HOST/testname2")
        self.assertTrue(res[0]["servicePrincipalName"][0] == "HOST/testname2.testdom" or
                        res[0]["servicePrincipalName"][1] == "HOST/testname2.testdom")

        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)

    def test_sam_description_attribute(self):
        """Test SAM description attribute"""
        print "Test SAM description attribute"""

        self.ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "description": "desc2",
            "objectclass": "group",
            "description": "desc1"})

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 1)
        self.assertEquals(res[0]["description"][0], "desc1")

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        self.ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "description": ["desc1", "desc2"]})

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 2)
        self.assertTrue(res[0]["description"][0] == "desc1" or
                        res[0]["description"][1] == "desc1")
        self.assertTrue(res[0]["description"][0] == "desc2" or
                        res[0]["description"][1] == "desc2")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement(["desc1","desc2"], FLAG_MOD_REPLACE,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement(["desc1","desc2"], FLAG_MOD_DELETE,
          "description")
        ldb.modify(m)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        self.ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group" })

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement("desc1", FLAG_MOD_REPLACE,
          "description")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 1)
        self.assertEquals(res[0]["description"][0], "desc1")

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        self.ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "description": ["desc1", "desc2"]})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement("desc1", FLAG_MOD_REPLACE,
          "description")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 1)
        self.assertEquals(res[0]["description"][0], "desc1")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement("desc3", FLAG_MOD_ADD,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement(["desc1","desc2"], FLAG_MOD_DELETE,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement("desc1", FLAG_MOD_DELETE,
          "description")
        ldb.modify(m)
        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("description" in res[0])

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement(["desc1","desc2"], FLAG_MOD_REPLACE,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement(["desc3", "desc4"], FLAG_MOD_ADD,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement("desc1", FLAG_MOD_ADD,
          "description")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 1)
        self.assertEquals(res[0]["description"][0], "desc1")

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m.add(MessageElement("desc1", FLAG_MOD_DELETE, "description"))
        m.add(MessageElement("desc2", FLAG_MOD_ADD, "description"))
        ldb.modify(m)

        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["description"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("description" in res[0])
        self.assertTrue(len(res[0]["description"]) == 1)
        self.assertEquals(res[0]["description"][0], "desc2")

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)


if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host

ldb = SamDB(host, credentials=creds, session_info=system_session(lp), lp=lp)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(SamTests)).wasSuccessful():
    rc = 1
sys.exit(rc)
