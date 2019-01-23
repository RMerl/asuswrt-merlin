#!/usr/bin/env python
# -*- coding: utf-8 -*-

import optparse
import sys
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import (LdbError, ERR_NO_SUCH_OBJECT, Message,
    MessageElement, Dn, FLAG_MOD_REPLACE)
from samba.samdb import SamDB
import samba.tests
import samba.dsdb as dsdb

from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("urgent_replication.py [options] <host>")
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

class UrgentReplicationTests(samba.tests.TestCase):

    def delete_force(self, ldb, dn):
        try:
            ldb.delete(dn, ["relax:0"])
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

    def setUp(self):
        super(UrgentReplicationTests, self).setUp()
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()

        print "baseDN: %s\n" % self.base_dn

    def test_nonurgent_object(self):
        """Test if the urgent replication is not activated
           when handling a non urgent object"""
        self.ldb.add({
            "dn": "cn=nonurgenttest,cn=users," + self.base_dn,
            "objectclass":"user",
            "samaccountname":"nonurgenttest",
            "description":"nonurgenttest description"})

        # urgent replication should not be enabled when creating 
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should not be enabled when modifying
        m = Message()
        m.dn = Dn(ldb, "cn=nonurgenttest,cn=users," + self.base_dn)
        m["description"] = MessageElement("new description", FLAG_MOD_REPLACE,
          "description")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should not be enabled when deleting
        self.delete_force(self.ldb, "cn=nonurgenttest,cn=users," + self.base_dn)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])


    def test_nTDSDSA_object(self):
        '''Test if the urgent replication is activated
           when handling a nTDSDSA object'''
        self.ldb.add({
            "dn": "cn=test server,cn=Servers,cn=Default-First-Site-Name,cn=Sites,cn=Configuration," + self.base_dn,
            "objectclass":"server",
            "cn":"test server",
            "name":"test server",
            "systemFlags":"50000000"}, ["relax:0"])

        self.ldb.add_ldif(
            """dn: cn=NTDS Settings test,cn=test server,cn=Servers,cn=Default-First-Site-Name,cn=Sites,cn=Configuration,%s""" % (self.base_dn) + """
objectclass: nTDSDSA
cn: NTDS Settings test
options: 1
instanceType: 4
systemFlags: 33554432""", ["relax:0"])

        # urgent replication should be enabled when creation
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when modifying
        m = Message()
        m.dn = Dn(ldb, "cn=NTDS Settings test,cn=test server,cn=Servers,cn=Default-First-Site-Name,cn=Sites,cn=Configuration," + self.base_dn)
        m["options"] = MessageElement("0", FLAG_MOD_REPLACE,
          "options")
        ldb.modify(m)
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when deleting
        self.delete_force(self.ldb, "cn=NTDS Settings test,cn=test server,cn=Servers,cn=Default-First-Site-Name,cn=Sites,cn=Configuration," + self.base_dn)
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        self.delete_force(self.ldb, "cn=test server,cn=Servers,cn=Default-First-Site-Name,cn=Sites,cn=Configuration," + self.base_dn)


    def test_crossRef_object(self):
        '''Test if the urgent replication is activated
           when handling a crossRef object'''
        self.ldb.add({
                      "dn": "CN=test crossRef,CN=Partitions,CN=Configuration,"+ self.base_dn,
                      "objectClass": "crossRef",
                      "cn": "test crossRef",
                      "dnsRoot": lp.get("realm").lower(),
                      "instanceType": "4",
                      "nCName": self.base_dn,
                      "showInAdvancedViewOnly": "TRUE",
                      "name": "test crossRef",
                      "systemFlags": "1"}, ["relax:0"])

        # urgent replication should be enabled when creating
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when modifying
        m = Message()
        m.dn = Dn(ldb, "cn=test crossRef,CN=Partitions,CN=Configuration," + self.base_dn)
        m["systemFlags"] = MessageElement("0", FLAG_MOD_REPLACE,
          "systemFlags")
        ldb.modify(m)
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])


        # urgent replication should be enabled when deleting
        self.delete_force(self.ldb, "cn=test crossRef,CN=Partitions,CN=Configuration," + self.base_dn)
        res = self.ldb.load_partition_usn("cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])



    def test_attributeSchema_object(self):
        '''Test if the urgent replication is activated
           when handling an attributeSchema object'''

        try:
            self.ldb.add_ldif(
                              """dn: CN=test attributeSchema,cn=Schema,CN=Configuration,%s""" % self.base_dn + """
objectClass: attributeSchema
cn: test attributeSchema
instanceType: 4
isSingleValued: FALSE
showInAdvancedViewOnly: FALSE
attributeID: 0.9.2342.19200300.100.1.1
attributeSyntax: 2.5.5.12
adminDisplayName: test attributeSchema
adminDescription: test attributeSchema
oMSyntax: 64
systemOnly: FALSE
searchFlags: 8
lDAPDisplayName: test attributeSchema
name: test attributeSchema""")

            # urgent replication should be enabled when creating
            res = self.ldb.load_partition_usn("cn=Schema,cn=Configuration," + self.base_dn)
            self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        except LdbError:
            print "Not testing urgent replication when creating attributeSchema object ...\n"

        # urgent replication should be enabled when modifying 
        m = Message()
        m.dn = Dn(ldb, "CN=test attributeSchema,CN=Schema,CN=Configuration," + self.base_dn)
        m["lDAPDisplayName"] = MessageElement("updated test attributeSchema", FLAG_MOD_REPLACE,
          "lDAPDisplayName")
        ldb.modify(m)
        res = self.ldb.load_partition_usn("cn=Schema,cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])


    def test_classSchema_object(self):
        '''Test if the urgent replication is activated
           when handling a classSchema object'''
        try:
            self.ldb.add_ldif(
                            """dn: CN=test classSchema,CN=Schema,CN=Configuration,%s""" % self.base_dn + """
objectClass: classSchema
cn: test classSchema
instanceType: 4
subClassOf: top
governsID: 1.2.840.113556.1.5.999
rDNAttID: cn
showInAdvancedViewOnly: TRUE
adminDisplayName: test classSchema
adminDescription: test classSchema
objectClassCategory: 1
lDAPDisplayName: test classSchema
name: test classSchema
systemOnly: FALSE
systemPossSuperiors: dfsConfiguration
systemMustContain: msDFS-SchemaMajorVersion
defaultSecurityDescriptor: D:(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;DA)(A;;RPWPCRCCD
 CLCLORCWOWDSDDTSW;;;SY)(A;;RPLCLORC;;;AU)(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;CO)
systemFlags: 16
defaultHidingValue: TRUE""")

            # urgent replication should be enabled when creating
            res = self.ldb.load_partition_usn("cn=Schema,cn=Configuration," + self.base_dn)
            self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        except LdbError:
            print "Not testing urgent replication when creating classSchema object ...\n"

        # urgent replication should be enabled when modifying 
        m = Message()
        m.dn = Dn(ldb, "CN=test classSchema,CN=Schema,CN=Configuration," + self.base_dn)
        m["lDAPDisplayName"] = MessageElement("updated test classSchema", FLAG_MOD_REPLACE,
          "lDAPDisplayName")
        ldb.modify(m)
        res = self.ldb.load_partition_usn("cn=Schema,cn=Configuration," + self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])


    def test_secret_object(self):
        '''Test if the urgent replication is activated
           when handling a secret object'''

        self.ldb.add({
            "dn": "cn=test secret,cn=System," + self.base_dn,
            "objectClass":"secret",
            "cn":"test secret",
            "name":"test secret",
            "currentValue":"xxxxxxx"})

        # urgent replication should be enabled when creating
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when modifying
        m = Message()
        m.dn = Dn(ldb, "cn=test secret,cn=System," + self.base_dn)
        m["currentValue"] = MessageElement("yyyyyyyy", FLAG_MOD_REPLACE,
          "currentValue")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when deleting 
        self.delete_force(self.ldb, "cn=test secret,cn=System," + self.base_dn)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])


    def test_rIDManager_object(self):
        '''Test if the urgent replication is activated
            when handling a rIDManager object'''
        self.ldb.add_ldif(
            """dn: CN=RID Manager test,CN=System,%s""" % self.base_dn + """
objectClass: rIDManager
cn: RID Manager test
instanceType: 4
showInAdvancedViewOnly: TRUE
name: RID Manager test
systemFlags: -1946157056
isCriticalSystemObject: TRUE
rIDAvailablePool: 133001-1073741823""", ["relax:0"])

        # urgent replication should be enabled when creating
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when modifying
        m = Message()
        m.dn = Dn(ldb, "CN=RID Manager test,CN=System," + self.base_dn)
        m["systemFlags"] = MessageElement("0", FLAG_MOD_REPLACE,
          "systemFlags")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when deleting 
        self.delete_force(self.ldb, "CN=RID Manager test,CN=System," + self.base_dn)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])


    def test_urgent_attributes(self):
        '''Test if the urgent replication is activated
            when handling urgent attributes of an object'''

        self.ldb.add({
            "dn": "cn=user UrgAttr test,cn=users," + self.base_dn,
            "objectclass":"user",
            "samaccountname":"user UrgAttr test",
            "userAccountControl":str(dsdb.UF_NORMAL_ACCOUNT),
            "lockoutTime":"0",
            "pwdLastSet":"0",
            "description":"urgent attributes test description"})

        # urgent replication should NOT be enabled when creating
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when modifying userAccountControl 
        m = Message()
        m.dn = Dn(ldb, "cn=user UrgAttr test,cn=users," + self.base_dn)
        m["userAccountControl"] = MessageElement(str(dsdb.UF_NORMAL_ACCOUNT+dsdb.UF_SMARTCARD_REQUIRED), FLAG_MOD_REPLACE,
          "userAccountControl")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when modifying lockoutTime
        m = Message()
        m.dn = Dn(ldb, "cn=user UrgAttr test,cn=users," + self.base_dn)
        m["lockoutTime"] = MessageElement("1", FLAG_MOD_REPLACE,
          "lockoutTime")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should be enabled when modifying pwdLastSet
        m = Message()
        m.dn = Dn(ldb, "cn=user UrgAttr test,cn=users," + self.base_dn)
        m["pwdLastSet"] = MessageElement("1", FLAG_MOD_REPLACE,
          "pwdLastSet")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when modifying a not-urgent
        # attribute
        m = Message()
        m.dn = Dn(ldb, "cn=user UrgAttr test,cn=users," + self.base_dn)
        m["description"] = MessageElement("updated urgent attributes test description",
                                          FLAG_MOD_REPLACE, "description")
        ldb.modify(m)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])

        # urgent replication should NOT be enabled when deleting
        self.delete_force(self.ldb, "cn=user UrgAttr test,cn=users," + self.base_dn)
        res = self.ldb.load_partition_usn(self.base_dn)
        self.assertNotEquals(res["uSNHighest"], res["uSNUrgent"])


if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host


ldb = SamDB(host, credentials=creds, session_info=system_session(lp), lp=lp,
            global_schema=False)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(UrgentReplicationTests)).wasSuccessful():
    rc = 1
sys.exit(rc)
