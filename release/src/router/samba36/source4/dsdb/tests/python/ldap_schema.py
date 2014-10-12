#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This is a port of the original in testprogs/ejs/ldap.js

import optparse
import sys
import time
import random
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import SCOPE_ONELEVEL, SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT
from ldb import ERR_UNWILLING_TO_PERFORM
from ldb import ERR_CONSTRAINT_VIOLATION
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_REPLACE
from samba.samdb import SamDB
from samba.dsdb import DS_DOMAIN_FUNCTION_2003
from samba.tests import delete_force

from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("ldap_schema.py [options] <host>")
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


class SchemaTests(unittest.TestCase):

    def setUp(self):
        super(SchemaTests, self).setUp()
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()
        self.schema_dn = ldb.get_schema_basedn().get_linearized()

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
        attr_name = "test-Attr" + time.strftime("%s", time.gmtime())
        attr_ldap_display_name = attr_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (attr_name, self.schema_dn) + """
objectClass: top
objectClass: attributeSchema
adminDescription: """ + attr_name + """
adminDisplayName: """ + attr_name + """
cn: """ + attr_name + """
attributeId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9940
attributeSyntax: 2.5.5.12
omSyntax: 64
instanceType: 4
isSingleValued: TRUE
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # Search for created attribute
        res = []
        res = self.ldb.search("cn=%s,%s" % (attr_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], attr_ldap_display_name)
        self.assertTrue("schemaIDGUID" in res[0])

        class_name = "test-Class" + time.strftime("%s", time.gmtime())
        class_ldap_display_name = class_name.replace("-", "")

        # First try to create a class with a wrong "defaultObjectCategory"
        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
objectClass: top
objectClass: classSchema
defaultObjectCategory: CN=_
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
objectClassCategory: 1
subClassOf: organizationalPerson
systemFlags: 16
rDNAttID: cn
systemMustContain: cn
systemMustContain: """ + attr_ldap_display_name + """
systemOnly: FALSE
"""
        try:
                 self.ldb.add_ldif(ldif)
                 self.fail()
        except LdbError, (num, _):
                 self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
objectClassCategory: 1
subClassOf: organizationalPerson
systemFlags: 16
rDNAttID: cn
systemMustContain: cn
systemMustContain: """ + attr_ldap_display_name + """
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # Search for created objectclass
        res = []
        res = self.ldb.search("cn=%s,%s" % (class_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], class_ldap_display_name)
        self.assertEquals(res[0]["defaultObjectCategory"][0], res[0]["distinguishedName"][0])
        self.assertTrue("schemaIDGUID" in res[0])

        ldif = """
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
"""
        self.ldb.modify_ldif(ldif)

        object_name = "obj" + time.strftime("%s", time.gmtime())

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
""" + attr_ldap_display_name + """: test
"""
        self.ldb.add_ldif(ldif)

        # Search for created object
        res = []
        res = self.ldb.search("cn=%s,cn=Users,%s" % (object_name, self.base_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        # Delete the object
        delete_force(self.ldb, "cn=%s,cn=Users,%s" % (object_name, self.base_dn))

    def test_subClassOf(self):
        """ Testing usage of custom child schamaClass
        """

        class_name = "my-Class" + time.strftime("%s", time.gmtime())
        class_ldap_display_name = class_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
objectClassCategory: 1
subClassOf: organizationalUnit
systemFlags: 16
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # Search for created objectclass
        res = []
        res = self.ldb.search("cn=%s,%s" % (class_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], class_ldap_display_name)
        self.assertEquals(res[0]["defaultObjectCategory"][0], res[0]["distinguishedName"][0])
        self.assertTrue("schemaIDGUID" in res[0])

        ldif = """
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
"""
        self.ldb.modify_ldif(ldif)

        object_name = "org" + time.strftime("%s", time.gmtime())

        ldif = """
dn: OU=%s,%s""" % (object_name, self.base_dn) + """
objectClass: """ + class_ldap_display_name + """
ou: """ + object_name + """
instanceType: 4
"""
        self.ldb.add_ldif(ldif)

        # Search for created object
        res = []
        res = self.ldb.search("ou=%s,%s" % (object_name, self.base_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        # Delete the object
        delete_force(self.ldb, "ou=%s,%s" % (object_name, self.base_dn))


class SchemaTests_msDS_IntId(unittest.TestCase):

    def setUp(self):
        super(SchemaTests_msDS_IntId, self).setUp()
        self.ldb = ldb
        res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.schema_dn = res[0]["schemaNamingContext"][0]
        self.base_dn = res[0]["defaultNamingContext"][0]
        self.forest_level = int(res[0]["forestFunctionality"][0])

    def _ldap_schemaUpdateNow(self):
        ldif = """
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
"""
        self.ldb.modify_ldif(ldif)

    def _make_obj_names(self, prefix):
        class_name = prefix + time.strftime("%s", time.gmtime())
        class_ldap_name = class_name.replace("-", "")
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        return (class_name, class_ldap_name, class_dn)

    def _is_schema_base_object(self, ldb_msg):
        """Test systemFlags for SYSTEM_FLAG_SCHEMA_BASE_OBJECT (16)"""
        systemFlags = 0
        if "systemFlags" in ldb_msg:
            systemFlags = int(ldb_msg["systemFlags"][0])
        return (systemFlags & 16) != 0

    def _make_attr_ldif(self, attr_name, attr_dn):
        ldif = """
dn: """ + attr_dn + """
objectClass: top
objectClass: attributeSchema
adminDescription: """ + attr_name + """
adminDisplayName: """ + attr_name + """
cn: """ + attr_name + """
attributeId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9940
attributeSyntax: 2.5.5.12
omSyntax: 64
instanceType: 4
isSingleValued: TRUE
systemOnly: FALSE
"""
        return ldif

    def test_msDS_IntId_on_attr(self):
        """Testing msDs-IntId creation for Attributes.
        See MS-ADTS - 3.1.1.Attributes

        This test should verify that:
        - Creating attribute with 'msDS-IntId' fails with ERR_UNWILLING_TO_PERFORM
        - Adding 'msDS-IntId' on existing attribute fails with ERR_CONSTRAINT_VIOLATION
        - Creating attribute with 'msDS-IntId' set and FLAG_SCHEMA_BASE_OBJECT flag
          set fails with ERR_UNWILLING_TO_PERFORM
        - Attributes created with FLAG_SCHEMA_BASE_OBJECT not set have
          'msDS-IntId' attribute added internally
        """

        # 1. Create attribute without systemFlags
        # msDS-IntId should be created if forest functional
        # level is >= DS_DOMAIN_FUNCTION_2003
        # and missing otherwise
        (attr_name, attr_ldap_name, attr_dn) = self._make_obj_names("msDS-IntId-Attr-1-")
        ldif = self._make_attr_ldif(attr_name, attr_dn)

        # try to add msDS-IntId during Attribute creation
        ldif_fail = ldif + "msDS-IntId: -1993108831\n"
        try:
            self.ldb.add_ldif(ldif_fail)
            self.fail("Adding attribute with preset msDS-IntId should fail")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # add the new attribute and update schema
        self.ldb.add_ldif(ldif)
        self._ldap_schemaUpdateNow()

        # Search for created attribute
        res = []
        res = self.ldb.search(attr_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], attr_ldap_name)
        if self.forest_level >= DS_DOMAIN_FUNCTION_2003:
            if self._is_schema_base_object(res[0]):
                self.assertTrue("msDS-IntId" not in res[0])
            else:
                self.assertTrue("msDS-IntId" in res[0])
        else:
            self.assertTrue("msDS-IntId" not in res[0])

        msg = Message()
        msg.dn = Dn(self.ldb, attr_dn)
        msg["msDS-IntId"] = MessageElement("-1993108831", FLAG_MOD_REPLACE, "msDS-IntId")
        try:
            self.ldb.modify(msg)
            self.fail("Modifying msDS-IntId should return error")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # 2. Create attribute with systemFlags = FLAG_SCHEMA_BASE_OBJECT
        # msDS-IntId should be created if forest functional
        # level is >= DS_DOMAIN_FUNCTION_2003
        # and missing otherwise
        (attr_name, attr_ldap_name, attr_dn) = self._make_obj_names("msDS-IntId-Attr-2-")
        ldif = self._make_attr_ldif(attr_name, attr_dn)
        ldif += "systemFlags: 16\n"

        # try to add msDS-IntId during Attribute creation
        ldif_fail = ldif + "msDS-IntId: -1993108831\n"
        try:
            self.ldb.add_ldif(ldif_fail)
            self.fail("Adding attribute with preset msDS-IntId should fail")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # add the new attribute and update schema
        self.ldb.add_ldif(ldif)
        self._ldap_schemaUpdateNow()

        # Search for created attribute
        res = []
        res = self.ldb.search(attr_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], attr_ldap_name)
        if self.forest_level >= DS_DOMAIN_FUNCTION_2003:
            if self._is_schema_base_object(res[0]):
                self.assertTrue("msDS-IntId" not in res[0])
            else:
                self.assertTrue("msDS-IntId" in res[0])
        else:
            self.assertTrue("msDS-IntId" not in res[0])

        msg = Message()
        msg.dn = Dn(self.ldb, attr_dn)
        msg["msDS-IntId"] = MessageElement("-1993108831", FLAG_MOD_REPLACE, "msDS-IntId")
        try:
            self.ldb.modify(msg)
            self.fail("Modifying msDS-IntId should return error")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)


    def _make_class_ldif(self, class_dn, class_name):
        ldif = """
dn: """ + class_dn + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
objectClassCategory: 1
subClassOf: organizationalPerson
rDNAttID: cn
systemMustContain: cn
systemOnly: FALSE
"""
        return ldif

    def test_msDS_IntId_on_class(self):
        """Testing msDs-IntId creation for Class
           Reference: MS-ADTS - 3.1.1.2.4.8 Class classSchema"""

        # 1. Create Class without systemFlags
        # msDS-IntId should be created if forest functional
        # level is >= DS_DOMAIN_FUNCTION_2003
        # and missing otherwise
        (class_name, class_ldap_name, class_dn) = self._make_obj_names("msDS-IntId-Class-1-")
        ldif = self._make_class_ldif(class_dn, class_name)

        # try to add msDS-IntId during Class creation
        ldif_add = ldif + "msDS-IntId: -1993108831\n"
        self.ldb.add_ldif(ldif_add)
        self._ldap_schemaUpdateNow()

        res = self.ldb.search(class_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["msDS-IntId"][0], "-1993108831")

        # add a new Class and update schema
        (class_name, class_ldap_name, class_dn) = self._make_obj_names("msDS-IntId-Class-2-")
        ldif = self._make_class_ldif(class_dn, class_name)

        self.ldb.add_ldif(ldif)
        self._ldap_schemaUpdateNow()

        # Search for created Class
        res = self.ldb.search(class_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertFalse("msDS-IntId" in res[0])

        msg = Message()
        msg.dn = Dn(self.ldb, class_dn)
        msg["msDS-IntId"] = MessageElement("-1993108831", FLAG_MOD_REPLACE, "msDS-IntId")
        try:
            self.ldb.modify(msg)
            self.fail("Modifying msDS-IntId should return error")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # 2. Create Class with systemFlags = FLAG_SCHEMA_BASE_OBJECT
        # msDS-IntId should be created if forest functional
        # level is >= DS_DOMAIN_FUNCTION_2003
        # and missing otherwise
        (class_name, class_ldap_name, class_dn) = self._make_obj_names("msDS-IntId-Class-3-")
        ldif = self._make_class_ldif(class_dn, class_name)
        ldif += "systemFlags: 16\n"

        # try to add msDS-IntId during Class creation
        ldif_add = ldif + "msDS-IntId: -1993108831\n"
        self.ldb.add_ldif(ldif_add)

        res = self.ldb.search(class_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["msDS-IntId"][0], "-1993108831")

        # add the new Class and update schema
        (class_name, class_ldap_name, class_dn) = self._make_obj_names("msDS-IntId-Class-4-")
        ldif = self._make_class_ldif(class_dn, class_name)
        ldif += "systemFlags: 16\n"

        self.ldb.add_ldif(ldif)
        self._ldap_schemaUpdateNow()

        # Search for created Class
        res = self.ldb.search(class_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertFalse("msDS-IntId" in res[0])

        msg = Message()
        msg.dn = Dn(self.ldb, class_dn)
        msg["msDS-IntId"] = MessageElement("-1993108831", FLAG_MOD_REPLACE, "msDS-IntId")
        try:
            self.ldb.modify(msg)
            self.fail("Modifying msDS-IntId should return error")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        res = self.ldb.search(class_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertFalse("msDS-IntId" in res[0])


    def test_verify_msDS_IntId(self):
        """Verify msDS-IntId exists only on attributes without FLAG_SCHEMA_BASE_OBJECT flag set"""
        count = 0
        res = self.ldb.search(self.schema_dn, scope=SCOPE_ONELEVEL,
                              expression="objectClass=attributeSchema",
                              attrs=["systemFlags", "msDS-IntId", "attributeID", "cn"])
        self.assertTrue(len(res) > 1)
        for ldb_msg in res:
            if self.forest_level >= DS_DOMAIN_FUNCTION_2003:
                if self._is_schema_base_object(ldb_msg):
                    self.assertTrue("msDS-IntId" not in ldb_msg)
                else:
                    # don't assert here as there are plenty of
                    # attributes under w2k8 that are not part of
                    # Base Schema (SYSTEM_FLAG_SCHEMA_BASE_OBJECT flag not set)
                    # has not msDS-IntId attribute set
                    #self.assertTrue("msDS-IntId" in ldb_msg, "msDS-IntId expected on: %s" % ldb_msg.dn)
                    if "msDS-IntId" not in ldb_msg:
                        count = count + 1
                        print "%3d warning: msDS-IntId expected on: %-30s %s" % (count, ldb_msg["attributeID"], ldb_msg["cn"])
            else:
                self.assertTrue("msDS-IntId" not in ldb_msg)


class SchemaTests_msDS_isRODC(unittest.TestCase):

    def setUp(self):
        super(SchemaTests_msDS_isRODC, self).setUp()
        self.ldb = ldb
        res = ldb.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.base_dn = res[0]["defaultNamingContext"][0]

    def test_objectClass_ntdsdsa(self):
        res = self.ldb.search(self.base_dn, expression="objectClass=nTDSDSA",
                              attrs=["msDS-isRODC"], controls=["search_options:1:2"])
        for ldb_msg in res:
            self.assertTrue("msDS-isRODC" in ldb_msg)

    def test_objectClass_server(self):
        res = self.ldb.search(self.base_dn, expression="objectClass=server",
                              attrs=["msDS-isRODC"], controls=["search_options:1:2"])
        for ldb_msg in res:
            ntds_search_dn = "CN=NTDS Settings,%s" % ldb_msg['dn']
            try:
                res_check = self.ldb.search(ntds_search_dn, attrs=["objectCategory"])
            except LdbError, (num, _):
                self.assertEquals(num, ERR_NO_SUCH_OBJECT)
                print("Server entry %s doesn't have a NTDS settings object" % res[0]['dn'])
            else:
                self.assertTrue("objectCategory" in res_check[0])
                self.assertTrue("msDS-isRODC" in ldb_msg)

    def test_objectClass_computer(self):
        res = self.ldb.search(self.base_dn, expression="objectClass=computer",
                              attrs=["serverReferenceBL","msDS-isRODC"], controls=["search_options:1:2"])
        for ldb_msg in res:
            if "serverReferenceBL" not in ldb_msg:
                print("Computer entry %s doesn't have a serverReferenceBL attribute" % ldb_msg['dn'])
            else:
                self.assertTrue("msDS-isRODC" in ldb_msg)

if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host

ldb_options = []
if host.startswith("ldap://"):
    # user 'paged_search' module when connecting remotely
    ldb_options = ["modules:paged_searches"]

ldb = SamDB(host, credentials=creds, session_info=system_session(lp), lp=lp, options=ldb_options)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(SchemaTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(SchemaTests_msDS_IntId)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(SchemaTests_msDS_isRODC)).wasSuccessful():
    rc = 1

sys.exit(rc)
