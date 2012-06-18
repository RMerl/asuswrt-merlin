#!/usr/bin/python
# -*- coding: utf-8 -*-
# This is a port of the original in testprogs/ejs/ldap.js

import getopt
import optparse
import sys
import time
import random
import base64

sys.path.append("bin/python")
sys.path.append("../lib/subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT, ERR_ATTRIBUTE_OR_VALUE_EXISTS
from ldb import ERR_ENTRY_ALREADY_EXISTS, ERR_UNWILLING_TO_PERFORM
from ldb import ERR_NOT_ALLOWED_ON_NON_LEAF, ERR_OTHER, ERR_INVALID_DN_SYNTAX
from ldb import Message, MessageElement, Dn, FLAG_MOD_REPLACE
from samba import Ldb, param, dom_sid_to_rid
from subunit import SubunitTestRunner
import unittest

from samba.ndr import ndr_pack, ndr_unpack
from samba.dcerpc import security

parser = optparse.OptionParser("ldap [options] <host>")
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

class BasicTests(unittest.TestCase):
    def delete_force(self, ldb, dn):
        try:
            ldb.delete(dn)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

    def find_basedn(self, ldb):
        res = ldb.search(base="", expression="", scope=SCOPE_BASE,
                         attrs=["defaultNamingContext"])
        self.assertEquals(len(res), 1)
        return res[0]["defaultNamingContext"][0]

    def find_configurationdn(self, ldb):
        res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["configurationNamingContext"])
        self.assertEquals(len(res), 1)
        return res[0]["configurationNamingContext"][0]

    def find_schemadn(self, ldb):
        res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["schemaNamingContext"])
        self.assertEquals(len(res), 1)
        return res[0]["schemaNamingContext"][0]

    def find_domain_sid(self):
        res = self.ldb.search(base=self.base_dn, expression="(objectClass=*)", scope=SCOPE_BASE)
        return ndr_unpack( security.dom_sid,res[0]["objectSid"][0])

    def setUp(self):
        self.ldb = ldb
        self.gc_ldb = gc_ldb
        self.base_dn = self.find_basedn(ldb)
        self.configuration_dn = self.find_configurationdn(ldb)
        self.schema_dn = self.find_schemadn(ldb)
        self.domain_sid = self.find_domain_sid()

        print "baseDN: %s\n" % self.base_dn

        self.delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestutf8user èùéìòà ,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestutf8user2  èùéìòà ,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=parentguidtest,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=parentguidtest,cn=testotherusers," + self.base_dn)
        self.delete_force(self.ldb, "cn=testotherusers," + self.base_dn)

    def test_group_add_invalid_member(self):
        """Testing group add with invalid member"""
        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=uSers," + self.base_dn,
                "objectclass": "group",
                "member": "cn=ldaptestuser,cn=useRs," + self.base_dn})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

    def test_parentGUID(self):
        """Test parentGUID behaviour"""
        print "Testing parentGUID behaviour\n"

        self.ldb.add({
            "dn": "cn=parentguidtest,cn=users," + self.base_dn,
            "objectclass":"user",
            "samaccountname":"parentguidtest"});
        res1 = ldb.search(base="cn=parentguidtest,cn=users," + self.base_dn, scope=SCOPE_BASE,
                          attrs=["parentGUID"]);
        res2 = ldb.search(base="cn=users," + self.base_dn,scope=SCOPE_BASE,
                          attrs=["objectGUID"]);
        self.assertEquals(res1[0]["parentGUID"], res2[0]["objectGUID"]);

        """Test parentGUID behaviour"""
        print "Testing parentGUID behaviour on rename\n"

        self.ldb.add({
            "dn": "cn=testotherusers," + self.base_dn,
            "objectclass":"container"});
        res1 = ldb.search(base="cn=testotherusers," + self.base_dn,scope=SCOPE_BASE,
                          attrs=["objectGUID"]);
        ldb.rename("cn=parentguidtest,cn=users," + self.base_dn,
                   "cn=parentguidtest,cn=testotherusers," + self.base_dn);
        res2 = ldb.search(base="cn=parentguidtest,cn=testotherusers," + self.base_dn,
                          scope=SCOPE_BASE,
                          attrs=["parentGUID"]);
        self.assertEquals(res1[0]["objectGUID"], res2[0]["parentGUID"]);
        ldb.delete("cn=parentguidtest,cn=testotherusers," + self.base_dn)
        ldb.delete("cn=testotherusers," + self.base_dn)

    def test_groupType(self):
        """Test groupType behaviour (should appear to be casted to a 32 bit signed integer before comparsion)"""
        print "Testing groupType behaviour\n"

        res1 = ldb.search(base=self.base_dn, scope=SCOPE_SUBTREE,
                          attrs=["groupType"], expression="groupType=2147483653");

        res2 = ldb.search(base=self.base_dn, scope=SCOPE_SUBTREE,
                          attrs=["groupType"], expression="groupType=-2147483643");

        self.assertEquals(len(res1), len(res2))

        self.assertTrue(res1.count > 0)

        self.assertEquals(res1[0]["groupType"][0], "-2147483643")

    def test_primary_group(self):
        """This tests the primary group behaviour (setting, changing) of a user account"""
        print "Testing primary group behaviour\n"

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": ["user", "person"]})

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        ldb.add({
            "dn": "cn=ldaptestgroup2,cn=users," + self.base_dn,
            "objectclass": "group"})

        res1 = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["objectSID"])
        self.assertTrue(len(res1) == 1)
	group_rid_1 = dom_sid_to_rid(ldb.schema_format_value("objectSID",
          res1[0]["objectSID"][0]))

        res1 = ldb.search("cn=ldaptestgroup2,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["objectSID"])
        self.assertTrue(len(res1) == 1)
        group_rid_2 = dom_sid_to_rid(ldb.schema_format_value("objectSID",
          res1[0]["objectSID"][0]))

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
        m["member"] = "cn=ldaptestuser,cn=users," + self.base_dn
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
        m["member"] = "cn=ldaptestuser,cn=users," + self.base_dn
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # Make group 2 secondary
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        m["member"] = "cn=ldaptestuser,cn=users," + self.base_dn
        ldb.modify(m)

        # Swap the groups
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
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

        # Also this should be denied
        try:
            ldb.add({
              "dn": "cn=ldaptestuser1,cn=users," + self.base_dn,
              "objectclass": ["user", "person"],
              "primaryGroupID": "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)

    def test_primary_group_token(self):
        """Test the primary group token behaviour (hidden-generated-readonly attribute on groups)"""
        print "Testing primary group token behaviour\n"

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": ["user", "person"]})

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

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

	rid = dom_sid_to_rid(ldb.schema_format_value("objectSID", res1[0]["objectSID"][0]))
        self.assertEquals(primary_group_token, rid)

# Has to wait until we support read-only generated attributes correctly
#        m = Message()
#        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
#        m["primaryGroupToken"] = "100"
#        try:
#                ldb.modify(m)
#                self.fail()
#        except LdbError, (num, msg):

        self.delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        self.delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_all(self):
        """Basic tests"""

        print "Testing user add"

        self.delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=uSers," + self.base_dn,
            "objectclass": ["user", "person"],
            "cN": "LDAPtestUSER",
            "givenname": "ldap",
            "sn": "testy"})

        self.delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=uSers," + self.base_dn,
            "objectclass": "group",
            "member": "cn=ldaptestuser,cn=useRs," + self.base_dn})

        self.delete_force(ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "cN": "LDAPtestCOMPUTER"})

        self.delete_force(self.ldb, "cn=ldaptest2computer,cn=computers," + self.base_dn)
        ldb.add({"dn": "cn=ldaptest2computer,cn=computers," + self.base_dn,
            "objectClass": "computer",
            "cn": "LDAPtest2COMPUTER",
            "userAccountControl": "4096",
            "displayname": "ldap testy"})

        self.delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        try:
            ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                     "objectClass": "computer",
                     "cn": "LDAPtest2COMPUTER"
                     })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        self.delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        try:
            ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                     "objectClass": "computer",
                     "cn": "ldaptestcomputer3",
                     "sAMAccountType": "805306368"
                })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        try:
            ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                     "objectClass": "computer",
                     "cn": "ldaptestcomputer3",
                     "userAccountControl": "0"
                })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.delete_force(self.ldb, "cn=ldaptestuser7,cn=users," + self.base_dn)
        try:
            ldb.add({"dn": "cn=ldaptestuser7,cn=users," + self.base_dn,
                     "objectClass": "user",
                     "cn": "LDAPtestuser7",
                     "userAccountControl": "0"
                })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.delete_force(self.ldb, "cn=ldaptestuser7,cn=users," + self.base_dn)

        ldb.add({"dn": "cn=ldaptestuser7,cn=users," + self.base_dn,
                 "objectClass": "user",
                 "cn": "LDAPtestuser7",
                 "userAccountControl": "2"
                 })

        self.delete_force(self.ldb, "cn=ldaptestuser7,cn=users," + self.base_dn)

        self.delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                 "objectClass": "computer",
                 "cn": "LDAPtestCOMPUTER3"
                 })

	print "Testing ldb.search for (&(cn=ldaptestcomputer3)(objectClass=user))";
        res = ldb.search(self.base_dn, expression="(&(cn=ldaptestcomputer3)(objectClass=user))");
        self.assertEquals(len(res), 1, "Found only %d for (&(cn=ldaptestcomputer3)(objectClass=user))" % len(res))

	self.assertEquals(str(res[0].dn), ("CN=ldaptestcomputer3,CN=Computers," + self.base_dn));
	self.assertEquals(res[0]["cn"][0], "ldaptestcomputer3");
	self.assertEquals(res[0]["name"][0], "ldaptestcomputer3");
	self.assertEquals(res[0]["objectClass"][0], "top");
	self.assertEquals(res[0]["objectClass"][1], "person");
	self.assertEquals(res[0]["objectClass"][2], "organizationalPerson");
	self.assertEquals(res[0]["objectClass"][3], "user");
	self.assertEquals(res[0]["objectClass"][4], "computer");
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("whenCreated" in res[0])
	self.assertEquals(res[0]["objectCategory"][0], ("CN=Computer,CN=Schema,CN=Configuration," + self.base_dn));
	self.assertEquals(int(res[0]["primaryGroupID"][0]), 513);
	self.assertEquals(int(res[0]["sAMAccountType"][0]), 805306368);
	self.assertEquals(int(res[0]["userAccountControl"][0]), 546);

        self.delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)

        print "Testing attribute or value exists behaviour"
        try:
            ldb.modify_ldif("""
dn: cn=ldaptest2computer,cn=computers,""" + self.base_dn + """
changetype: modify
replace: servicePrincipalName
servicePrincipalName: host/ldaptest2computer
servicePrincipalName: host/ldaptest2computer
servicePrincipalName: cifs/ldaptest2computer
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        ldb.modify_ldif("""
dn: cn=ldaptest2computer,cn=computers,""" + self.base_dn + """
changetype: modify
replace: servicePrincipalName
servicePrincipalName: host/ldaptest2computer
servicePrincipalName: cifs/ldaptest2computer
""")
        try:
            ldb.modify_ldif("""
dn: cn=ldaptest2computer,cn=computers,""" + self.base_dn + """
changetype: modify
add: servicePrincipalName
servicePrincipalName: host/ldaptest2computer
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        print "Testing ranged results"
        ldb.modify_ldif("""
dn: cn=ldaptest2computer,cn=computers,""" + self.base_dn + """
changetype: modify
replace: servicePrincipalName
""")

        ldb.modify_ldif("""
dn: cn=ldaptest2computer,cn=computers,""" + self.base_dn + """
changetype: modify
add: servicePrincipalName
servicePrincipalName: host/ldaptest2computer0
servicePrincipalName: host/ldaptest2computer1
servicePrincipalName: host/ldaptest2computer2
servicePrincipalName: host/ldaptest2computer3
servicePrincipalName: host/ldaptest2computer4
servicePrincipalName: host/ldaptest2computer5
servicePrincipalName: host/ldaptest2computer6
servicePrincipalName: host/ldaptest2computer7
servicePrincipalName: host/ldaptest2computer8
servicePrincipalName: host/ldaptest2computer9
servicePrincipalName: host/ldaptest2computer10
servicePrincipalName: host/ldaptest2computer11
servicePrincipalName: host/ldaptest2computer12
servicePrincipalName: host/ldaptest2computer13
servicePrincipalName: host/ldaptest2computer14
servicePrincipalName: host/ldaptest2computer15
servicePrincipalName: host/ldaptest2computer16
servicePrincipalName: host/ldaptest2computer17
servicePrincipalName: host/ldaptest2computer18
servicePrincipalName: host/ldaptest2computer19
servicePrincipalName: host/ldaptest2computer20
servicePrincipalName: host/ldaptest2computer21
servicePrincipalName: host/ldaptest2computer22
servicePrincipalName: host/ldaptest2computer23
servicePrincipalName: host/ldaptest2computer24
servicePrincipalName: host/ldaptest2computer25
servicePrincipalName: host/ldaptest2computer26
servicePrincipalName: host/ldaptest2computer27
servicePrincipalName: host/ldaptest2computer28
servicePrincipalName: host/ldaptest2computer29
""")

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE,
                         attrs=["servicePrincipalName;range=0-*"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        #print len(res[0]["servicePrincipalName;range=0-*"])
        self.assertEquals(len(res[0]["servicePrincipalName;range=0-*"]), 30)

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=0-19"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
            # print res[0]["servicePrincipalName;range=0-19"].length
        self.assertEquals(len(res[0]["servicePrincipalName;range=0-19"]), 20)


        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=0-30"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=0-*"]), 30)

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=0-40"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=0-*"]), 30)

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=30-40"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=30-*"]), 0)


        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=10-40"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=10-*"]), 20)
        # pos_11 = res[0]["servicePrincipalName;range=10-*"][18]

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=11-40"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=11-*"]), 19)
            # print res[0]["servicePrincipalName;range=11-*"][18]
            # print pos_11
            # self.assertEquals((res[0]["servicePrincipalName;range=11-*"][18]), pos_11)

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName;range=11-15"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
        self.assertEquals(len(res[0]["servicePrincipalName;range=11-15"]), 5)
            # self.assertEquals(res[0]["servicePrincipalName;range=11-15"][4], pos_11)

        res = ldb.search(self.base_dn, expression="(cn=ldaptest2computer))", scope=SCOPE_SUBTREE, attrs=["servicePrincipalName"])
        self.assertEquals(len(res), 1, "Could not find (cn=ldaptest2computer)")
            # print res[0]["servicePrincipalName"][18]
            # print pos_11
        self.assertEquals(len(res[0]["servicePrincipalName"]), 30)
            # self.assertEquals(res[0]["servicePrincipalName"][18], pos_11)

        self.delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        ldb.add({
            "dn": "cn=ldaptestuser2,cn=useRs," + self.base_dn,
            "objectClass": ["person", "user"],
            "cn": "LDAPtestUSER2",
            "givenname": "testy",
            "sn": "ldap user2"})

        print "Testing Ambigious Name Resolution"
        # Testing ldb.search for (&(anr=ldap testy)(objectClass=user))
        res = ldb.search(expression="(&(anr=ldap testy)(objectClass=user))")
        self.assertEquals(len(res), 3, "Found only %d of 3 for (&(anr=ldap testy)(objectClass=user))" % len(res))

        # Testing ldb.search for (&(anr=testy ldap)(objectClass=user))
        res = ldb.search(expression="(&(anr=testy ldap)(objectClass=user))")
        self.assertEquals(len(res), 2, "Found only %d of 2 for (&(anr=testy ldap)(objectClass=user))" % len(res))

        # Testing ldb.search for (&(anr=ldap)(objectClass=user))
        res = ldb.search(expression="(&(anr=ldap)(objectClass=user))")
        self.assertEquals(len(res), 4, "Found only %d of 4 for (&(anr=ldap)(objectClass=user))" % len(res))

        # Testing ldb.search for (&(anr==ldap)(objectClass=user))
        res = ldb.search(expression="(&(anr==ldap)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(anr==ldap)(objectClass=user)). Found only %d for (&(anr=ldap)(objectClass=user))" % len(res))

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
        self.assertEquals(res[0]["cn"][0], "ldaptestuser")
        self.assertEquals(str(res[0]["name"]), "ldaptestuser")

        # Testing ldb.search for (&(anr=testy)(objectClass=user))
        res = ldb.search(expression="(&(anr=testy)(objectClass=user))")
        self.assertEquals(len(res), 2, "Found only %d for (&(anr=testy)(objectClass=user))" % len(res))

        # Testing ldb.search for (&(anr=testy ldap)(objectClass=user))
        res = ldb.search(expression="(&(anr=testy ldap)(objectClass=user))")
        self.assertEquals(len(res), 2, "Found only %d for (&(anr=testy ldap)(objectClass=user))" % len(res))

        # Testing ldb.search for (&(anr==testy ldap)(objectClass=user))
# this test disabled for the moment, as anr with == tests are not understood
#        res = ldb.search(expression="(&(anr==testy ldap)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Found only %d for (&(anr==testy ldap)(objectClass=user))" % len(res))

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
        self.assertEquals(res[0]["cn"][0], "ldaptestuser")
        self.assertEquals(res[0]["name"][0], "ldaptestuser")

        # Testing ldb.search for (&(anr==testy ldap)(objectClass=user))
#        res = ldb.search(expression="(&(anr==testy ldap)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Could not find (&(anr==testy ldap)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
        self.assertEquals(res[0]["cn"][0], "ldaptestuser")
        self.assertEquals(res[0]["name"][0], "ldaptestuser")

        # Testing ldb.search for (&(anr=testy ldap user)(objectClass=user))
        res = ldb.search(expression="(&(anr=testy ldap user)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(anr=testy ldap user)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser2,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestuser2")
        self.assertEquals(str(res[0]["name"]), "ldaptestuser2")

        # Testing ldb.search for (&(anr==testy ldap user2)(objectClass=user))
#        res = ldb.search(expression="(&(anr==testy ldap user2)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Could not find (&(anr==testy ldap user2)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser2,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestuser2")
        self.assertEquals(str(res[0]["name"]), "ldaptestuser2")

        # Testing ldb.search for (&(anr==ldap user2)(objectClass=user))
#        res = ldb.search(expression="(&(anr==ldap user2)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Could not find (&(anr==ldap user2)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser2,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestuser2")
        self.assertEquals(str(res[0]["name"]), "ldaptestuser2")

        # Testing ldb.search for (&(anr==not ldap user2)(objectClass=user))
#        res = ldb.search(expression="(&(anr==not ldap user2)(objectClass=user))")
#        self.assertEquals(len(res), 0, "Must not find (&(anr==not ldap user2)(objectClass=user))")

        # Testing ldb.search for (&(anr=not ldap user2)(objectClass=user))
        res = ldb.search(expression="(&(anr=not ldap user2)(objectClass=user))")
        self.assertEquals(len(res), 0, "Must not find (&(anr=not ldap user2)(objectClass=user))")

        # Testing ldb.search for (&(anr="testy ldap")(objectClass=user)) (ie, with quotes)
#        res = ldb.search(expression="(&(anr==\"testy ldap\")(objectClass=user))")
#        self.assertEquals(len(res), 0, "Found (&(anr==\"testy ldap\")(objectClass=user))")

        print "Testing Group Modifies"
        ldb.modify_ldif("""
dn: cn=ldaptestgroup,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: cn=ldaptestuser2,cn=users,""" + self.base_dn + """
member: cn=ldaptestcomputer,cn=computers,""" + self.base_dn + """
""")

        self.delete_force(ldb, "cn=ldaptestuser3,cn=users," + self.base_dn)

        print "Testing adding non-existent user to a group"
        try:
            ldb.modify_ldif("""
dn: cn=ldaptestgroup,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: cn=ldaptestuser3,cn=users,""" + self.base_dn + """
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        print "Testing Renames"

        attrs = ["objectGUID", "objectSid"]
        print "Testing ldb.search for (&(cn=ldaptestUSer2)(objectClass=user))"
        res_user = ldb.search(self.base_dn, expression="(&(cn=ldaptestUSer2)(objectClass=user))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res_user), 1, "Could not find (&(cn=ldaptestUSer2)(objectClass=user))")

        #Check rename works with extended/alternate DN forms
        ldb.rename("<SID=" + ldb.schema_format_value("objectSID", res_user[0]["objectSID"][0]) + ">" , "cn=ldaptestuser3,cn=users," + self.base_dn)

        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser3,cn=users," + self.base_dn)

        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestUSER3,cn=users," + self.base_dn)

        print "Testing ldb.search for (&(cn=ldaptestuser3)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestuser3)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestuser3)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestUSER3,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestUSER3")
        self.assertEquals(str(res[0]["name"]), "ldaptestUSER3")

 	#"Testing ldb.search for (&(&(cn=ldaptestuser3)(userAccountControl=*))(objectClass=user))"
	res = ldb.search(expression="(&(&(cn=ldaptestuser3)(userAccountControl=*))(objectClass=user))")
        self.assertEquals(len(res), 1, "(&(&(cn=ldaptestuser3)(userAccountControl=*))(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestUSER3,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestUSER3")
        self.assertEquals(str(res[0]["name"]), "ldaptestUSER3")

 	#"Testing ldb.search for (&(&(cn=ldaptestuser3)(userAccountControl=546))(objectClass=user))"
	res = ldb.search(expression="(&(&(cn=ldaptestuser3)(userAccountControl=546))(objectClass=user))")
        self.assertEquals(len(res), 1, "(&(&(cn=ldaptestuser3)(userAccountControl=546))(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestUSER3,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestUSER3")
        self.assertEquals(str(res[0]["name"]), "ldaptestUSER3")

 	#"Testing ldb.search for (&(&(cn=ldaptestuser3)(userAccountControl=547))(objectClass=user))"
	res = ldb.search(expression="(&(&(cn=ldaptestuser3)(userAccountControl=547))(objectClass=user))")
        self.assertEquals(len(res), 0, "(&(&(cn=ldaptestuser3)(userAccountControl=547))(objectClass=user))")

        # This is a Samba special, and does not exist in real AD
        #    print "Testing ldb.search for (dn=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")"
        #    res = ldb.search("(dn=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")")
        #    if (res.error != 0 || len(res) != 1) {
        #        print "Could not find (dn=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")"
        #        self.assertEquals(len(res), 1)
        #    }
        #    self.assertEquals(res[0].dn, ("CN=ldaptestUSER3,CN=Users," + self.base_dn))
        #    self.assertEquals(res[0].cn, "ldaptestUSER3")
        #    self.assertEquals(res[0].name, "ldaptestUSER3")

        print "Testing ldb.search for (distinguishedName=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")"
        res = ldb.search(expression="(distinguishedName=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")")
        self.assertEquals(len(res), 1, "Could not find (dn=CN=ldaptestUSER3,CN=Users," + self.base_dn + ")")
        self.assertEquals(str(res[0].dn), ("CN=ldaptestUSER3,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestUSER3")
        self.assertEquals(str(res[0]["name"]), "ldaptestUSER3")

        # ensure we cannot add it again
        try:
            ldb.add({"dn": "cn=ldaptestuser3,cn=userS," + self.base_dn,
                      "objectClass": ["person", "user"],
                      "cn": "LDAPtestUSER3"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # rename back
        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser2,cn=users," + self.base_dn)

        # ensure we cannnot rename it twice
        try:
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn,
                       "cn=ldaptestuser2,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # ensure can now use that name
        ldb.add({"dn": "cn=ldaptestuser3,cn=users," + self.base_dn,
                      "objectClass": ["person", "user"],
                      "cn": "LDAPtestUSER3"})

        # ensure we now cannnot rename
        try:
            ldb.rename("cn=ldaptestuser2,cn=users," + self.base_dn, "cn=ldaptestuser3,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)
        try:
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser3,cn=configuration," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num in (71, 64))

        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser5,cn=users," + self.base_dn)

        ldb.delete("cn=ldaptestuser5,cn=users," + self.base_dn)

        self.delete_force(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)

        ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn, "cn=ldaptestgroup2,cn=users," + self.base_dn)

        print "Testing subtree Renames"

        ldb.add({"dn": "cn=ldaptestcontainer," + self.base_dn,
                 "objectClass": "container"})

        self.delete_force(self.ldb, "cn=ldaptestuser4,cn=ldaptestcontainer," + self.base_dn)
        ldb.add({"dn": "CN=ldaptestuser4,CN=ldaptestcontainer," + self.base_dn,
                 "objectClass": ["person", "user"],
                 "cn": "LDAPtestUSER4"})

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: cn=ldaptestuser4,cn=ldaptestcontainer,""" + self.base_dn + """
""")

        print "Testing ldb.rename of cn=ldaptestcontainer," + self.base_dn + " to cn=ldaptestcontainer2," + self.base_dn
        ldb.rename("CN=ldaptestcontainer," + self.base_dn, "CN=ldaptestcontainer2," + self.base_dn)

        print "Testing ldb.search for (&(cn=ldaptestuser4)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestuser4)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestuser4)(objectClass=user))")

        print "Testing subtree ldb.search for (&(cn=ldaptestuser4)(objectClass=user)) in (just renamed from) cn=ldaptestcontainer," + self.base_dn
        try:
            res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                    expression="(&(cn=ldaptestuser4)(objectClass=user))",
                    scope=SCOPE_SUBTREE)
            self.fail(res)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        print "Testing one-level ldb.search for (&(cn=ldaptestuser4)(objectClass=user)) in (just renamed from) cn=ldaptestcontainer," + self.base_dn
        try:
            res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                    expression="(&(cn=ldaptestuser4)(objectClass=user))", scope=SCOPE_ONELEVEL)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        print "Testing ldb.search for (&(cn=ldaptestuser4)(objectClass=user)) in renamed container"
        res = ldb.search("cn=ldaptestcontainer2," + self.base_dn, expression="(&(cn=ldaptestuser4)(objectClass=user))", scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestuser4)(objectClass=user)) under cn=ldaptestcontainer2," + self.base_dn)

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn))
        self.assertEquals(res[0]["memberOf"][0].upper(), ("CN=ldaptestgroup2,CN=Users," + self.base_dn).upper())

        time.sleep(4)

        print "Testing ldb.search for (&(member=CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn + ")(objectclass=group)) to check subtree renames and linked attributes"
        res = ldb.search(self.base_dn, expression="(&(member=CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn + ")(objectclass=group))", scope=SCOPE_SUBTREE)
        self.assertEquals(len(res), 1, "Could not find (&(member=CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn + ")(objectclass=group)), perhaps linked attributes are not consistant with subtree renames?")

        print "Testing ldb.rename (into itself) of cn=ldaptestcontainer2," + self.base_dn + " to cn=ldaptestcontainer,cn=ldaptestcontainer2," + self.base_dn
        try:
            ldb.rename("cn=ldaptestcontainer2," + self.base_dn, "cn=ldaptestcontainer,cn=ldaptestcontainer2," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        print "Testing ldb.rename (into non-existent container) of cn=ldaptestcontainer2," + self.base_dn + " to cn=ldaptestcontainer,cn=ldaptestcontainer3," + self.base_dn
        try:
            ldb.rename("cn=ldaptestcontainer2," + self.base_dn, "cn=ldaptestcontainer,cn=ldaptestcontainer3," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num in (ERR_UNWILLING_TO_PERFORM, ERR_OTHER))

        print "Testing delete (should fail, not a leaf node) of renamed cn=ldaptestcontainer2," + self.base_dn
        try:
            ldb.delete("cn=ldaptestcontainer2," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)

        print "Testing base ldb.search for CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn
        res = ldb.search(expression="(objectclass=*)", base=("CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn), scope=SCOPE_BASE)
        self.assertEquals(len(res), 1)
        res = ldb.search(expression="(cn=ldaptestuser40)", base=("CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn), scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)

        print "Testing one-level ldb.search for (&(cn=ldaptestuser4)(objectClass=user)) in cn=ldaptestcontainer2," + self.base_dn
        res = ldb.search(expression="(&(cn=ldaptestuser4)(objectClass=user))", base=("cn=ldaptestcontainer2," + self.base_dn), scope=SCOPE_ONELEVEL)
        # FIXME: self.assertEquals(len(res), 0)

        print "Testing one-level ldb.search for (&(cn=ldaptestuser4)(objectClass=user)) in cn=ldaptestcontainer2," + self.base_dn
        res = ldb.search(expression="(&(cn=ldaptestuser4)(objectClass=user))", base=("cn=ldaptestcontainer2," + self.base_dn), scope=SCOPE_SUBTREE)
        # FIXME: self.assertEquals(len(res), 0)

        print "Testing delete of subtree renamed "+("CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn)
        ldb.delete(("CN=ldaptestuser4,CN=ldaptestcontainer2," + self.base_dn))
        print "Testing delete of renamed cn=ldaptestcontainer2," + self.base_dn
        ldb.delete("cn=ldaptestcontainer2," + self.base_dn)

        self.delete_force(self.ldb, "cn=ldaptestutf8user èùéìòà ,cn=users," + self.base_dn)
        ldb.add({"dn": "cn=ldaptestutf8user èùéìòà ,cn=users," + self.base_dn, "objectClass": "user"})

        self.delete_force(self.ldb, "cn=ldaptestutf8user2  èùéìòà ,cn=users," + self.base_dn)
        ldb.add({"dn": "cn=ldaptestutf8user2  èùéìòà ,cn=users," + self.base_dn, "objectClass": "user"})

        print "Testing ldb.search for (&(cn=ldaptestuser)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestuser)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestuser)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestuser")
        self.assertEquals(str(res[0]["name"]), "ldaptestuser")
        self.assertEquals(set(res[0]["objectClass"]), set(["top", "person", "organizationalPerson", "user"]))
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertEquals(str(res[0]["objectCategory"]), ("CN=Person,CN=Schema,CN=Configuration," + self.base_dn))
        self.assertEquals(int(res[0]["sAMAccountType"][0]), 805306368)
        self.assertEquals(int(res[0]["userAccountControl"][0]), 546)
        self.assertEquals(res[0]["memberOf"][0].upper(), ("CN=ldaptestgroup2,CN=Users," + self.base_dn).upper())
        self.assertEquals(len(res[0]["memberOf"]), 1)

        print "Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + self.base_dn + "))"
        res2 = ldb.search(expression="(&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + self.base_dn + "))")
        self.assertEquals(len(res2), 1, "Could not find (&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + self.base_dn + "))")

        self.assertEquals(res[0].dn, res2[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=PerSon))"
        res3 = ldb.search(expression="(&(cn=ldaptestuser)(objectCategory=PerSon))")
        self.assertEquals(len(res3), 1, "Could not find (&(cn=ldaptestuser)(objectCategory=PerSon)): matched %d" % len(res3))

        self.assertEquals(res[0].dn, res3[0].dn)

        if gc_ldb is not None:
            print "Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=PerSon)) in Global Catalog"
            res3gc = gc_ldb.search(expression="(&(cn=ldaptestuser)(objectCategory=PerSon))")
            self.assertEquals(len(res3gc), 1)

            self.assertEquals(res[0].dn, res3gc[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=PerSon)) in with 'phantom root' control"

        res3control = gc_ldb.search(self.base_dn, expression="(&(cn=ldaptestuser)(objectCategory=PerSon))", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["search_options:1:2"])
        self.assertEquals(len(res3control), 1, "Could not find (&(cn=ldaptestuser)(objectCategory=PerSon)) in Global Catalog")

        self.assertEquals(res[0].dn, res3control[0].dn)

        ldb.delete(res[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestcomputer)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestcomputer)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestuser)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestcomputer,CN=Computers," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestcomputer")
        self.assertEquals(str(res[0]["name"]), "ldaptestcomputer")
        self.assertEquals(set(res[0]["objectClass"]), set(["top", "person", "organizationalPerson", "user", "computer"]))
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertEquals(str(res[0]["objectCategory"]), ("CN=Computer,CN=Schema,CN=Configuration," + self.base_dn))
        self.assertEquals(int(res[0]["primaryGroupID"][0]), 513)
        self.assertEquals(int(res[0]["sAMAccountType"][0]), 805306368)
        self.assertEquals(int(res[0]["userAccountControl"][0]), 546)
        self.assertEquals(res[0]["memberOf"][0].upper(), ("CN=ldaptestgroup2,CN=Users," + self.base_dn).upper())
        self.assertEquals(len(res[0]["memberOf"]), 1)

        print "Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + "))"
        res2 = ldb.search(expression="(&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + "))")
        self.assertEquals(len(res2), 1, "Could not find (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + "))")

        self.assertEquals(res[0].dn, res2[0].dn)

        if gc_ldb is not None:
            print "Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + ")) in Global Catlog"
            res2gc = gc_ldb.search(expression="(&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + "))")
            self.assertEquals(len(res2gc), 1, "Could not find (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + self.base_dn + ")) in Global Catlog")

            self.assertEquals(res[0].dn, res2gc[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=compuTER))"
        res3 = ldb.search(expression="(&(cn=ldaptestcomputer)(objectCategory=compuTER))")
        self.assertEquals(len(res3), 1, "Could not find (&(cn=ldaptestcomputer)(objectCategory=compuTER))")

        self.assertEquals(res[0].dn, res3[0].dn)

        if gc_ldb is not None:
            print "Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=compuTER)) in Global Catalog"
            res3gc = gc_ldb.search(expression="(&(cn=ldaptestcomputer)(objectCategory=compuTER))")
            self.assertEquals(len(res3gc), 1, "Could not find (&(cn=ldaptestcomputer)(objectCategory=compuTER)) in Global Catalog")

            self.assertEquals(res[0].dn, res3gc[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestcomp*r)(objectCategory=compuTER))"
        res4 = ldb.search(expression="(&(cn=ldaptestcomp*r)(objectCategory=compuTER))")
        self.assertEquals(len(res4), 1, "Could not find (&(cn=ldaptestcomp*r)(objectCategory=compuTER))")

        self.assertEquals(res[0].dn, res4[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestcomput*)(objectCategory=compuTER))"
        res5 = ldb.search(expression="(&(cn=ldaptestcomput*)(objectCategory=compuTER))")
        self.assertEquals(len(res5), 1, "Could not find (&(cn=ldaptestcomput*)(objectCategory=compuTER))")

        self.assertEquals(res[0].dn, res5[0].dn)

        print "Testing ldb.search for (&(cn=*daptestcomputer)(objectCategory=compuTER))"
        res6 = ldb.search(expression="(&(cn=*daptestcomputer)(objectCategory=compuTER))")
        self.assertEquals(len(res6), 1, "Could not find (&(cn=*daptestcomputer)(objectCategory=compuTER))")

        self.assertEquals(res[0].dn, res6[0].dn)

        ldb.delete("<GUID=" + ldb.schema_format_value("objectGUID", res[0]["objectGUID"][0]) + ">")

        print "Testing ldb.search for (&(cn=ldaptest2computer)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptest2computer)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptest2computer)(objectClass=user))")

        self.assertEquals(str(res[0].dn), "CN=ldaptest2computer,CN=Computers," + self.base_dn)
        self.assertEquals(str(res[0]["cn"]), "ldaptest2computer")
        self.assertEquals(str(res[0]["name"]), "ldaptest2computer")
        self.assertEquals(list(res[0]["objectClass"]), ["top", "person", "organizationalPerson", "user", "computer"])
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertEquals(res[0]["objectCategory"][0], "CN=Computer,CN=Schema,CN=Configuration," + self.base_dn)
        self.assertEquals(int(res[0]["sAMAccountType"][0]), 805306369)
        self.assertEquals(int(res[0]["userAccountControl"][0]), 4096)

        ldb.delete("<SID=" + ldb.schema_format_value("objectSID", res[0]["objectSID"][0]) + ">")

        attrs = ["cn", "name", "objectClass", "objectGUID", "objectSID", "whenCreated", "nTSecurityDescriptor", "memberOf", "allowedAttributes", "allowedAttributesEffective"]
        print "Testing ldb.search for (&(cn=ldaptestUSer2)(objectClass=user))"
        res_user = ldb.search(self.base_dn, expression="(&(cn=ldaptestUSer2)(objectClass=user))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res_user), 1, "Could not find (&(cn=ldaptestUSer2)(objectClass=user))")

        self.assertEquals(str(res_user[0].dn), ("CN=ldaptestuser2,CN=Users," + self.base_dn))
        self.assertEquals(str(res_user[0]["cn"]), "ldaptestuser2")
        self.assertEquals(str(res_user[0]["name"]), "ldaptestuser2")
        self.assertEquals(list(res_user[0]["objectClass"]), ["top", "person", "organizationalPerson", "user"])
        self.assertTrue("objectSid" in res_user[0])
        self.assertTrue("objectGUID" in res_user[0])
        self.assertTrue("whenCreated" in res_user[0])
        self.assertTrue("nTSecurityDescriptor" in res_user[0])
        self.assertTrue("allowedAttributes" in res_user[0])
        self.assertTrue("allowedAttributesEffective" in res_user[0])
        self.assertEquals(res_user[0]["memberOf"][0].upper(), ("CN=ldaptestgroup2,CN=Users," + self.base_dn).upper())

        ldaptestuser2_sid = res_user[0]["objectSid"][0]
        ldaptestuser2_guid = res_user[0]["objectGUID"][0]

        attrs = ["cn", "name", "objectClass", "objectGUID", "objectSID", "whenCreated", "nTSecurityDescriptor", "member", "allowedAttributes", "allowedAttributesEffective"]
        print "Testing ldb.search for (&(cn=ldaptestgroup2)(objectClass=group))"
        res = ldb.search(self.base_dn, expression="(&(cn=ldaptestgroup2)(objectClass=group))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestgroup2)(objectClass=group))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestgroup2,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestgroup2")
        self.assertEquals(str(res[0]["name"]), "ldaptestgroup2")
        self.assertEquals(list(res[0]["objectClass"]), ["top", "group"])
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("objectSid" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertTrue("nTSecurityDescriptor" in res[0])
        self.assertTrue("allowedAttributes" in res[0])
        self.assertTrue("allowedAttributesEffective" in res[0])
        memberUP = []
        for m in res[0]["member"]:
            memberUP.append(m.upper())
        self.assertTrue(("CN=ldaptestuser2,CN=Users," + self.base_dn).upper() in memberUP)

        res = ldb.search(self.base_dn, expression="(&(cn=ldaptestgroup2)(objectClass=group))", scope=SCOPE_SUBTREE, attrs=attrs, controls=["extended_dn:1:1"])
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestgroup2)(objectClass=group))")

        print res[0]["member"]
        memberUP = []
        for m in res[0]["member"]:
            memberUP.append(m.upper())
        print ("<GUID=" + ldb.schema_format_value("objectGUID", ldaptestuser2_guid) + ">;<SID=" + ldb.schema_format_value("objectSid", ldaptestuser2_sid) + ">;CN=ldaptestuser2,CN=Users," + self.base_dn).upper()

        self.assertTrue(("<GUID=" + ldb.schema_format_value("objectGUID", ldaptestuser2_guid) + ">;<SID=" + ldb.schema_format_value("objectSid", ldaptestuser2_sid) + ">;CN=ldaptestuser2,CN=Users," + self.base_dn).upper() in memberUP)

        print "Testing Linked attribute behaviours"
        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
replace: member
member: CN=ldaptestuser2,CN=Users,""" + self.base_dn + """
member: CN=ldaptestutf8user èùéìòà,CN=Users,""" + self.base_dn + """
""")

        ldb.modify_ldif("""
dn: <GUID=""" + ldb.schema_format_value("objectGUID", res[0]["objectGUID"][0]) + """>
changetype: modify
replace: member
member: CN=ldaptestutf8user èùéìòà,CN=Users,""" + self.base_dn + """
""")

        ldb.modify_ldif("""
dn: <SID=""" + ldb.schema_format_value("objectSid", res[0]["objectSid"][0]) + """>
changetype: modify
delete: member
""")

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: <GUID=""" + ldb.schema_format_value("objectGUID", res[0]["objectGUID"][0]) + """>
member: CN=ldaptestutf8user èùéìòà,CN=Users,""" + self.base_dn + """
""")

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
replace: member
""")

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: <SID=""" + ldb.schema_format_value("objectSid", res_user[0]["objectSid"][0]) + """>
member: CN=ldaptestutf8user èùéìòà,CN=Users,""" + self.base_dn + """
""")

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
delete: member
member: CN=ldaptestutf8user èùéìòà,CN=Users,""" + self.base_dn + """
""")

        res = ldb.search(self.base_dn, expression="(&(cn=ldaptestgroup2)(objectClass=group))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestgroup2)(objectClass=group))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestgroup2,CN=Users," + self.base_dn))
        self.assertEquals(res[0]["member"][0], ("CN=ldaptestuser2,CN=Users," + self.base_dn))
        self.assertEquals(len(res[0]["member"]), 1)

        ldb.delete(("CN=ldaptestuser2,CN=Users," + self.base_dn))

        time.sleep(4)

        attrs = ["cn", "name", "objectClass", "objectGUID", "whenCreated", "nTSecurityDescriptor", "member"]
        print "Testing ldb.search for (&(cn=ldaptestgroup2)(objectClass=group)) to check linked delete"
        res = ldb.search(self.base_dn, expression="(&(cn=ldaptestgroup2)(objectClass=group))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestgroup2)(objectClass=group)) to check linked delete")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestgroup2,CN=Users," + self.base_dn))
        self.assertTrue("member" not in res[0])

        print "Testing ldb.search for (&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")

        self.assertEquals(str(res[0].dn), ("CN=ldaptestutf8user èùéìòà,CN=Users," + self.base_dn))
        self.assertEquals(str(res[0]["cn"]), "ldaptestutf8user èùéìòà")
        self.assertEquals(str(res[0]["name"]), "ldaptestutf8user èùéìòà")
        self.assertEquals(list(res[0]["objectClass"]), ["top", "person", "organizationalPerson", "user"])
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("whenCreated" in res[0])

        ldb.delete(res[0].dn)

        print "Testing ldb.search for (&(cn=ldaptestutf8user2*)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestutf8user2*)(objectClass=user))")
        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestutf8user2*)(objectClass=user))")

        ldb.delete(res[0].dn)

        ldb.delete(("CN=ldaptestgroup2,CN=Users," + self.base_dn))

        print "Testing ldb.search for (&(cn=ldaptestutf8user2 ÈÙÉÌÒÀ)(objectClass=user))"
        res = ldb.search(expression="(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")

        #FIXME: self.assert len(res) == 1, "Could not find (expect space collapse, win2k3 fails) (&(cn=ldaptestutf8user2 ÈÙÉÌÒÀ)(objectClass=user))"

        print "Testing that we can't get at the configuration DN from the main search base"
        res = ldb.search(self.base_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertEquals(len(res), 0)

        print "Testing that we can get at the configuration DN from the main search base on the LDAP port with the 'phantom root' search_options control"
        res = ldb.search(self.base_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["search_options:1:2"])
        self.assertTrue(len(res) > 0)

        if gc_ldb is not None:
            print "Testing that we can get at the configuration DN from the main search base on the GC port with the search_options control == 0"

            res = gc_ldb.search(self.base_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["search_options:1:0"])
            self.assertTrue(len(res) > 0)

            print "Testing that we do find configuration elements in the global catlog"
            res = gc_ldb.search(self.base_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"])
            self.assertTrue(len(res) > 0)

            print "Testing that we do find configuration elements and user elements at the same time"
            res = gc_ldb.search(self.base_dn, expression="(|(objectClass=crossRef)(objectClass=person))", scope=SCOPE_SUBTREE, attrs=["cn"])
            self.assertTrue(len(res) > 0)

            print "Testing that we do find configuration elements in the global catlog, with the configuration basedn"
            res = gc_ldb.search(self.configuration_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"])
            self.assertTrue(len(res) > 0)

        print "Testing that we can get at the configuration DN on the main LDAP port"
        res = ldb.search(self.configuration_dn, expression="objectClass=crossRef", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0)

        print "Testing objectCategory canonacolisation"
        res = ldb.search(self.configuration_dn, expression="objectCategory=ntDsDSA", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0, "Didn't find any records with objectCategory=ntDsDSA")
        self.assertTrue(len(res) != 0)

        res = ldb.search(self.configuration_dn, expression="objectCategory=CN=ntDs-DSA," + self.schema_dn, scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0, "Didn't find any records with objectCategory=CN=ntDs-DSA," + self.schema_dn)
        self.assertTrue(len(res) != 0)

        print "Testing objectClass attribute order on "+ self.base_dn
        res = ldb.search(expression="objectClass=domain", base=self.base_dn,
                         scope=SCOPE_BASE, attrs=["objectClass"])
        self.assertEquals(len(res), 1)

        self.assertEquals(list(res[0]["objectClass"]), ["top", "domain", "domainDNS"])

    #  check enumeration

        print "Testing ldb.search for objectCategory=person"
        res = ldb.search(self.base_dn, expression="objectCategory=person", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0)

        print "Testing ldb.search for objectCategory=person with domain scope control"
        res = ldb.search(self.base_dn, expression="objectCategory=person", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["domain_scope:1"])
        self.assertTrue(len(res) > 0)

        print "Testing ldb.search for objectCategory=user"
        res = ldb.search(self.base_dn, expression="objectCategory=user", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0)

        print "Testing ldb.search for objectCategory=user with domain scope control"
        res = ldb.search(self.base_dn, expression="objectCategory=user", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["domain_scope:1"])
        self.assertTrue(len(res) > 0)

        print "Testing ldb.search for objectCategory=group"
        res = ldb.search(self.base_dn, expression="objectCategory=group", scope=SCOPE_SUBTREE, attrs=["cn"])
        self.assertTrue(len(res) > 0)

        print "Testing ldb.search for objectCategory=group with domain scope control"
        res = ldb.search(self.base_dn, expression="objectCategory=group", scope=SCOPE_SUBTREE, attrs=["cn"], controls=["domain_scope:1"])
        self.assertTrue(len(res) > 0)

    def test_security_descriptor_add(self):
        """ Testing ldb.add_ldif() for nTSecurityDescriptor """
        user_name = "testdescriptoruser1"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        #
        # Test add_ldif() with SDDL security descriptor input
        #
        self.delete_force(self.ldb, user_dn)
        try:
            sddl = "O:DUG:DUD:PAI(A;;RPWP;;;AU)S:PAI"
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name + """
nTSecurityDescriptor: """ + sddl)
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack( security.descriptor, desc )
            desc_sddl = desc.as_sddl( self.domain_sid )
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)
        #
        # Test add_ldif() with BASE64 security descriptor
        #
        try:
            sddl = "O:DUG:DUD:PAI(A;;RPWP;;;AU)S:PAI"
            desc = security.descriptor.from_sddl(sddl, self.domain_sid)
            desc_binary = ndr_pack(desc)
            desc_base64 = base64.b64encode(desc_binary)
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name + """
nTSecurityDescriptor:: """ + desc_base64)
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)

    def test_security_descriptor_add_neg(self):
        """Test add_ldif() with BASE64 security descriptor input using WRONG domain SID
            Negative test
        """
        user_name = "testdescriptoruser1"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        self.delete_force(self.ldb, user_dn)
        try:
            sddl = "O:DUG:DUD:PAI(A;;RPWP;;;AU)S:PAI"
            desc = security.descriptor.from_sddl(sddl, security.dom_sid('S-1-5-21'))
            desc_base64 = base64.b64encode( ndr_pack(desc) )
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name + """
nTSecurityDescriptor:: """ + desc_base64)
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            print res
            self.assertRaises(KeyError, lambda: res[0]["nTSecurityDescriptor"])
        finally:
            self.delete_force(self.ldb, user_dn)

    def test_security_descriptor_modify(self):
        """ Testing ldb.modify_ldif() for nTSecurityDescriptor """
        user_name = "testdescriptoruser2"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        #
        # Delete user object and test modify_ldif() with SDDL security descriptor input
        # Add ACE to the original descriptor test
        #
        try:
            self.delete_force(self.ldb, user_dn)
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name)
            # Modify descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            sddl = desc_sddl[:desc_sddl.find("(")] + "(A;;RPWP;;;AU)" + desc_sddl[desc_sddl.find("("):]
            mod = """
dn: """ + user_dn + """
changetype: modify
replace: nTSecurityDescriptor
nTSecurityDescriptor: """ + sddl
            self.ldb.modify_ldif(mod)
            # Read modified descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)
        #
        # Test modify_ldif() with SDDL security descriptor input
        # New desctiptor test
        #
        try:
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name)
            # Modify descriptor
            sddl = "O:DUG:DUD:PAI(A;;RPWP;;;AU)S:PAI"
            mod = """
dn: """ + user_dn + """
changetype: modify
replace: nTSecurityDescriptor
nTSecurityDescriptor: """ + sddl
            self.ldb.modify_ldif(mod)
            # Read modified descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)
        #
        # Test modify_ldif() with BASE64 security descriptor input
        # Add ACE to the original descriptor test
        #
        try:
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name)
            # Modify descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            sddl = desc_sddl[:desc_sddl.find("(")] + "(A;;RPWP;;;AU)" + desc_sddl[desc_sddl.find("("):]
            desc = security.descriptor.from_sddl(sddl, self.domain_sid)
            desc_base64 = base64.b64encode(ndr_pack(desc))
            mod = """
dn: """ + user_dn + """
changetype: modify
replace: nTSecurityDescriptor
nTSecurityDescriptor:: """ + desc_base64
            self.ldb.modify_ldif(mod)
            # Read modified descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)
        #
        # Test modify_ldif() with BASE64 security descriptor input
        # New descriptor test
        #
        try:
            self.delete_force(self.ldb, user_dn)
            self.ldb.add_ldif("""
dn: """ + user_dn + """
objectclass: user
sAMAccountName: """ + user_name)
            # Modify descriptor
            sddl = "O:DUG:DUD:PAI(A;;RPWP;;;AU)S:PAI"
            desc = security.descriptor.from_sddl(sddl, self.domain_sid)
            desc_base64 = base64.b64encode(ndr_pack(desc))
            mod = """
dn: """ + user_dn + """
changetype: modify
replace: nTSecurityDescriptor
nTSecurityDescriptor:: """ + desc_base64
            self.ldb.modify_ldif(mod)
            # Read modified descriptor
            res = self.ldb.search(base=user_dn, attrs=["nTSecurityDescriptor"])
            desc = res[0]["nTSecurityDescriptor"][0]
            desc = ndr_unpack(security.descriptor, desc)
            desc_sddl = desc.as_sddl(self.domain_sid)
            self.assertEqual(desc_sddl, sddl)
        finally:
            self.delete_force(self.ldb, user_dn)

class BaseDnTests(unittest.TestCase):
    def setUp(self):
        self.ldb = ldb

    def test_rootdse_attrs(self):
        """Testing for all rootDSE attributes"""
        res = self.ldb.search(scope=SCOPE_BASE, attrs=[])
        self.assertEquals(len(res), 1)

    def test_highestcommittedusn(self):
        """Testing for highestCommittedUSN"""
        res = self.ldb.search("", scope=SCOPE_BASE, attrs=["highestCommittedUSN"])
        self.assertEquals(len(res), 1)
        self.assertTrue(int(res[0]["highestCommittedUSN"][0]) != 0)

    def test_netlogon(self):
        """Testing for netlogon via LDAP"""
        res = self.ldb.search("", scope=SCOPE_BASE, attrs=["netlogon"])
        self.assertEquals(len(res), 0)

    def test_netlogon_highestcommitted_usn(self):
        """Testing for netlogon and highestCommittedUSN via LDAP"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                attrs=["netlogon", "highestCommittedUSN"])
        self.assertEquals(len(res), 0)

class SchemaTests(unittest.TestCase):
    def delete_force(self, ldb, dn):
        try:
            ldb.delete(dn)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

    def find_schemadn(self, ldb):
        res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["schemaNamingContext"])
        self.assertEquals(len(res), 1)
        return res[0]["schemaNamingContext"][0]

    def find_basedn(self, ldb):
        res = ldb.search(base="", expression="", scope=SCOPE_BASE,
                         attrs=["defaultNamingContext"])
        self.assertEquals(len(res), 1)
        return res[0]["defaultNamingContext"][0]

    def setUp(self):
        self.ldb = ldb
        self.schema_dn = self.find_schemadn(ldb)
        self.base_dn = self.find_basedn(ldb)

    def test_generated_schema(self):
        """Testing we can read the generated schema via LDAP"""
        res = self.ldb.search("cn=aggregate,"+self.schema_dn, scope=SCOPE_BASE,
                attrs=["objectClasses", "attributeTypes", "dITContentRules"])
        self.assertEquals(len(res), 1)
        self.assertTrue("dITContentRules" in res[0])
        self.assertTrue("objectClasses" in res[0])
        self.assertTrue("attributeTypes" in res[0])

    def test_generated_schema_is_operational(self):
        """Testing we don't get the generated schema via LDAP by default"""
        res = self.ldb.search("cn=aggregate,"+self.schema_dn, scope=SCOPE_BASE,
                attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertFalse("dITContentRules" in res[0])
        self.assertFalse("objectClasses" in res[0])
        self.assertFalse("attributeTypes" in res[0])

    def test_schemaUpdateNow(self):
        """Testing schemaUpdateNow"""
        class_name = "test-class" + time.strftime("%s", time.gmtime())
        class_ldap_display_name = class_name.replace("-", "")
        object_name = "obj" + time.strftime("%s", time.gmtime())

        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
lDAPDisplayName: """ + class_ldap_display_name + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
objectCategory: CN=Class-Schema,""" + self.schema_dn + """
defaultObjectCategory: CN=%s,%s""" % (class_name, self.schema_dn) + """
distinguishedName: CN=%s,%s""" % (class_name, self.schema_dn) + """
governsID: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
name: """ + class_name + """
objectClassCategory: 1
subClassOf: organizationalPerson
systemFlags: 16
rDNAttID: cn
systemMustContain: cn
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)
        ldif = """
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
"""
        self.ldb.modify_ldif(ldif)
        ldif = """
dn: CN=%s,CN=Users,%s"""% (object_name, self.base_dn) + """
objectClass: organizationalPerson
objectClass: person
objectClass: """ + class_ldap_display_name + """
objectClass: top
cn: """ + object_name + """
instanceType: 4
objectCategory: CN=%s,%s"""% (class_name, self.schema_dn) + """
distinguishedName: CN=%s,CN=Users,%s"""% (object_name, self.base_dn) + """
name: """ + object_name + """
"""
        self.ldb.add_ldif(ldif)
        # Search for created objectClass
        res = []
        res = self.ldb.search("cn=%s,%s" % (class_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertNotEqual(res, [])

        res = []
        res = self.ldb.search("cn=%s,cn=Users,%s" % (object_name, self.base_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertNotEqual(res, [])
        # Delete the object
        self.delete_force(self.ldb, "cn=%s,cn=Users,%s" % (object_name, self.base_dn))

if not "://" in host:
    host = "ldap://%s" % host

ldb = Ldb(host, credentials=creds, session_info=system_session(), lp=lp)
gc_ldb = Ldb("%s:3268" % host, credentials=creds,
             session_info=system_session(), lp=lp)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(BaseDnTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(BasicTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(SchemaTests)).wasSuccessful():
    rc = 1
sys.exit(rc)
