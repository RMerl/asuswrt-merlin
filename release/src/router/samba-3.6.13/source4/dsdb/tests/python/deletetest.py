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
from ldb import SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT, ERR_NOT_ALLOWED_ON_NON_LEAF
from ldb import ERR_UNWILLING_TO_PERFORM
from samba.samdb import SamDB
from samba.tests import delete_force

from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("deletetest.py [options] <host|file>")
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

class BasicDeleteTests(unittest.TestCase):


    def GUID_string(self, guid):
        return self.ldb.schema_format_value("objectGUID", guid)

    def setUp(self):
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()
        self.configuration_dn = ldb.get_config_basedn().get_linearized()

    def search_guid(self, guid):
        print "SEARCH by GUID %s" % self.GUID_string(guid)

        res = ldb.search(base="<GUID=%s>" % self.GUID_string(guid),
                         scope=SCOPE_BASE, controls=["show_deleted:1"])
        self.assertEquals(len(res), 1)
        return res[0]

    def search_dn(self,dn):
        print "SEARCH by DN %s" % dn

        res = ldb.search(expression="(objectClass=*)",
                         base=dn,
                         scope=SCOPE_BASE,
                         controls=["show_deleted:1"])
        self.assertEquals(len(res), 1)
        return res[0]

    def del_attr_values(self, delObj):
        print "Checking attributes for %s" % delObj["dn"]

        self.assertEquals(delObj["isDeleted"][0],"TRUE")
        self.assertTrue(not("objectCategory" in delObj))
        self.assertTrue(not("sAMAccountType" in delObj))

    def preserved_attributes_list(self, liveObj, delObj):
        print "Checking for preserved attributes list"

        preserved_list = ["nTSecurityDescriptor", "attributeID", "attributeSyntax", "dNReferenceUpdate", "dNSHostName",
        "flatName", "governsID", "groupType", "instanceType", "lDAPDisplayName", "legacyExchangeDN",
        "isDeleted", "isRecycled", "lastKnownParent", "msDS-LastKnownRDN", "mS-DS-CreatorSID",
        "mSMQOwnerID", "nCName", "objectClass", "distinguishedName", "objectGUID", "objectSid",
        "oMSyntax", "proxiedObjectName", "name", "replPropertyMetaData", "sAMAccountName",
        "securityIdentifier", "sIDHistory", "subClassOf", "systemFlags", "trustPartner", "trustDirection",
        "trustType", "trustAttributes", "userAccountControl", "uSNChanged", "uSNCreated", "whenCreated"]

        for a in liveObj:
            if a in preserved_list:
                self.assertTrue(a in delObj)

    def check_rdn(self, liveObj, delObj, rdnName):
        print "Checking for correct rDN"
        rdn=liveObj[rdnName][0]
        rdn2=delObj[rdnName][0]
        name2=delObj[rdnName][0]
        guid=liveObj["objectGUID"][0]
        self.assertEquals(rdn2, rdn + "\nDEL:" + self.GUID_string(guid))
        self.assertEquals(name2, rdn + "\nDEL:" + self.GUID_string(guid))

    def delete_deleted(self, ldb, dn):
        print "Testing the deletion of the already deleted dn %s" % dn

        try:
            ldb.delete(dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

    def test_delete_protection(self):
        """Delete protection tests"""

        print self.base_dn

        delete_force(self.ldb, "cn=entry1,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=entry2,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestcontainer," + self.base_dn,
            "objectclass": "container"})
        ldb.add({
            "dn": "cn=entry1,cn=ldaptestcontainer," + self.base_dn,
            "objectclass": "container"})
        ldb.add({
            "dn": "cn=entry2,cn=ldaptestcontainer," + self.base_dn,
            "objectclass": "container"})

        try:
            ldb.delete("cn=ldaptestcontainer," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)

        ldb.delete("cn=ldaptestcontainer," + self.base_dn, ["tree_delete:1"])

        try:
            res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                             scope=SCOPE_BASE, attrs=[])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)
        try:
            res = ldb.search("cn=entry1,cn=ldaptestcontainer," + self.base_dn,
                             scope=SCOPE_BASE, attrs=[])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)
        try:
            res = ldb.search("cn=entry2,cn=ldaptestcontainer," + self.base_dn,
                             scope=SCOPE_BASE, attrs=[])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        delete_force(self.ldb, "cn=entry1,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=entry2,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

        # Performs some protected object delete testing

        res = ldb.search(base="", expression="", scope=SCOPE_BASE,
                         attrs=["dsServiceName", "dNSHostName"])
        self.assertEquals(len(res), 1)

        # Delete failing since DC's nTDSDSA object is protected
        try:
            ldb.delete(res[0]["dsServiceName"][0])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        res = ldb.search(self.base_dn, attrs=["rIDSetReferences"],
                         expression="(&(objectClass=computer)(dNSHostName=" + res[0]["dNSHostName"][0] + "))")
        self.assertEquals(len(res), 1)

        # Deletes failing since DC's rIDSet object is protected
        try:
            ldb.delete(res[0]["rIDSetReferences"][0])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        try:
            ldb.delete(res[0]["rIDSetReferences"][0], ["tree_delete:1"])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Deletes failing since three main crossRef objects are protected

        try:
            ldb.delete("cn=Enterprise Schema,cn=Partitions," + self.configuration_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)
        try:
            ldb.delete("cn=Enterprise Schema,cn=Partitions," + self.configuration_dn, ["tree_delete:1"])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            ldb.delete("cn=Enterprise Configuration,cn=Partitions," + self.configuration_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)
        try:
            ldb.delete("cn=Enterprise Configuration,cn=Partitions," + self.configuration_dn, ["tree_delete:1"])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)

        res = ldb.search("cn=Partitions," + self.configuration_dn, attrs=[],
                         expression="(nCName=%s)" % self.base_dn)
        self.assertEquals(len(res), 1)

        try:
            ldb.delete(res[0].dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)
        try:
            ldb.delete(res[0].dn, ["tree_delete:1"])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_NON_LEAF)

        # Delete failing since "SYSTEM_FLAG_DISALLOW_DELETE"
        try:
            ldb.delete("CN=Users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Tree-delete failing since "isCriticalSystemObject"
        try:
            ldb.delete("CN=Computers," + self.base_dn, ["tree_delete:1"])
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

    def test_all(self):
        """Basic delete tests"""

        print self.base_dn

        usr1="cn=testuser,cn=users," + self.base_dn
        usr2="cn=testuser2,cn=users," + self.base_dn
        grp1="cn=testdelgroup1,cn=users," + self.base_dn
        sit1="cn=testsite1,cn=sites," + self.configuration_dn
        ss1="cn=NTDS Site Settings,cn=testsite1,cn=sites," + self.configuration_dn
        srv1="cn=Servers,cn=testsite1,cn=sites," + self.configuration_dn
        srv2="cn=TESTSRV,cn=Servers,cn=testsite1,cn=sites," + self.configuration_dn

        delete_force(self.ldb, usr1)
        delete_force(self.ldb, usr2)
        delete_force(self.ldb, grp1)
        delete_force(self.ldb, ss1)
        delete_force(self.ldb, srv2)
        delete_force(self.ldb, srv1)
        delete_force(self.ldb, sit1)

        ldb.add({
            "dn": usr1,
            "objectclass": "user",
            "description": "test user description",
            "samaccountname": "testuser"})

        ldb.add({
            "dn": usr2,
            "objectclass": "user",
            "description": "test user 2 description",
            "samaccountname": "testuser2"})

        ldb.add({
            "dn": grp1,
            "objectclass": "group",
            "description": "test group",
            "samaccountname": "testdelgroup1",
            "member": [ usr1, usr2 ],
            "isDeleted": "FALSE" })

        ldb.add({
            "dn": sit1,
            "objectclass": "site" })

        ldb.add({
            "dn": ss1,
            "objectclass": ["applicationSiteSettings", "nTDSSiteSettings"] })

        ldb.add({
            "dn": srv1,
            "objectclass": "serversContainer" })

        ldb.add({
            "dn": srv2,
            "objectClass": "server" })

        objLive1 = self.search_dn(usr1)
        guid1=objLive1["objectGUID"][0]

        objLive2 = self.search_dn(usr2)
        guid2=objLive2["objectGUID"][0]

        objLive3 = self.search_dn(grp1)
        guid3=objLive3["objectGUID"][0]

        objLive4 = self.search_dn(sit1)
        guid4=objLive4["objectGUID"][0]

        objLive5 = self.search_dn(ss1)
        guid5=objLive5["objectGUID"][0]

        objLive6 = self.search_dn(srv1)
        guid6=objLive6["objectGUID"][0]

        objLive7 = self.search_dn(srv2)
        guid7=objLive7["objectGUID"][0]

        ldb.delete(usr1)
        ldb.delete(usr2)
        ldb.delete(grp1)
        ldb.delete(srv1, ["tree_delete:1"])
        ldb.delete(sit1, ["tree_delete:1"])

        objDeleted1 = self.search_guid(guid1)
        objDeleted2 = self.search_guid(guid2)
        objDeleted3 = self.search_guid(guid3)
        objDeleted4 = self.search_guid(guid4)
        objDeleted5 = self.search_guid(guid5)
        objDeleted6 = self.search_guid(guid6)
        objDeleted7 = self.search_guid(guid7)

        self.del_attr_values(objDeleted1)
        self.del_attr_values(objDeleted2)
        self.del_attr_values(objDeleted3)
        self.del_attr_values(objDeleted4)
        self.del_attr_values(objDeleted5)
        self.del_attr_values(objDeleted6)
        self.del_attr_values(objDeleted7)

        self.preserved_attributes_list(objLive1, objDeleted1)
        self.preserved_attributes_list(objLive2, objDeleted2)
        self.preserved_attributes_list(objLive3, objDeleted3)
        self.preserved_attributes_list(objLive4, objDeleted4)
        self.preserved_attributes_list(objLive5, objDeleted5)
        self.preserved_attributes_list(objLive6, objDeleted6)
        self.preserved_attributes_list(objLive7, objDeleted7)

        self.check_rdn(objLive1, objDeleted1, "cn")
        self.check_rdn(objLive2, objDeleted2, "cn")
        self.check_rdn(objLive3, objDeleted3, "cn")
        self.check_rdn(objLive4, objDeleted4, "cn")
        self.check_rdn(objLive5, objDeleted5, "cn")
        self.check_rdn(objLive6, objDeleted6, "cn")
        self.check_rdn(objLive7, objDeleted7, "cn")

        self.delete_deleted(ldb, usr1)
        self.delete_deleted(ldb, usr2)
        self.delete_deleted(ldb, grp1)
        self.delete_deleted(ldb, sit1)
        self.delete_deleted(ldb, ss1)
        self.delete_deleted(ldb, srv1)
        self.delete_deleted(ldb, srv2)

        self.assertTrue("CN=Deleted Objects" in str(objDeleted1.dn))
        self.assertTrue("CN=Deleted Objects" in str(objDeleted2.dn))
        self.assertTrue("CN=Deleted Objects" in str(objDeleted3.dn))
        self.assertFalse("CN=Deleted Objects" in str(objDeleted4.dn))
        self.assertTrue("CN=Deleted Objects" in str(objDeleted5.dn))
        self.assertFalse("CN=Deleted Objects" in str(objDeleted6.dn))
        self.assertFalse("CN=Deleted Objects" in str(objDeleted7.dn))

if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host

ldb = SamDB(host, credentials=creds, session_info=system_session(lp), lp=lp)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(BasicDeleteTests)).wasSuccessful():
    rc = 1

sys.exit(rc)
