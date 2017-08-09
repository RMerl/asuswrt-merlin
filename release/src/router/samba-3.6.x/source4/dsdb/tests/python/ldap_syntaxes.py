#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Tests for LDAP syntaxes

import optparse
import sys
import time
import random
import uuid

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import SCOPE_BASE, SCOPE_SUBTREE, LdbError
from ldb import ERR_CONSTRAINT_VIOLATION
from ldb import ERR_INVALID_ATTRIBUTE_SYNTAX
from ldb import ERR_ENTRY_ALREADY_EXISTS

from subunit.run import SubunitTestRunner
import unittest

import samba.tests

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


class SyntaxTests(unittest.TestCase):

    def setUp(self):
        super(SyntaxTests, self).setUp()
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()
        self.schema_dn = ldb.get_schema_basedn().get_linearized()
        self._setup_dn_string_test()
        self._setup_dn_binary_test()

    def _setup_dn_string_test(self):
        """Testing DN+String syntax"""
        attr_name = "test-Attr-DN-String" + time.strftime("%s", time.gmtime())
        attr_ldap_display_name = attr_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (attr_name, self.schema_dn) + """
ldapDisplayName: """ + attr_ldap_display_name + """
objectClass: top
objectClass: attributeSchema
cn: """ + attr_name + """
attributeId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9940
attributeSyntax: 2.5.5.14
omSyntax: 127
omObjectClass: \x2A\x86\x48\x86\xF7\x14\x01\x01\x01\x0C
isSingleValued: FALSE
schemaIdGuid: """ + str(uuid.uuid4()) + """
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # search for created attribute
        res = []
        res = self.ldb.search("cn=%s,%s" % (attr_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], attr_ldap_display_name)
        self.assertTrue("schemaIDGUID" in res[0])

        class_name = "test-Class-DN-String" + time.strftime("%s", time.gmtime())
        class_ldap_display_name = class_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
schemaIdGuid: """ + str(uuid.uuid4()) + """
objectClassCategory: 1
subClassOf: organizationalPerson
systemMayContain: """ + attr_ldap_display_name + """
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # search for created objectclass
        res = []
        res = self.ldb.search("cn=%s,%s" % (class_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], class_ldap_display_name)
        self.assertEquals(res[0]["defaultObjectCategory"][0], res[0]["distinguishedName"][0])
        self.assertTrue("schemaIDGUID" in res[0])

        # store the class and the attribute
        self.dn_string_class_ldap_display_name = class_ldap_display_name
        self.dn_string_attribute = attr_ldap_display_name
        self.dn_string_class_name = class_name

    def _setup_dn_binary_test(self):
        """Testing DN+Binary syntaxes"""
        attr_name = "test-Attr-DN-Binary" + time.strftime("%s", time.gmtime())
        attr_ldap_display_name = attr_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (attr_name, self.schema_dn) + """
ldapDisplayName: """ + attr_ldap_display_name + """
objectClass: top
objectClass: attributeSchema
cn: """ + attr_name + """
attributeId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9940
attributeSyntax: 2.5.5.7
omSyntax: 127
omObjectClass: \x2A\x86\x48\x86\xF7\x14\x01\x01\x01\x0B
isSingleValued: FALSE
schemaIdGuid: """ + str(uuid.uuid4()) + """
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # search for created attribute
        res = []
        res = self.ldb.search("cn=%s,%s" % (attr_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], attr_ldap_display_name)
        self.assertTrue("schemaIDGUID" in res[0])

        class_name = "test-Class-DN-Binary" + time.strftime("%s", time.gmtime())
        class_ldap_display_name = class_name.replace("-", "")

        ldif = """
dn: CN=%s,%s""" % (class_name, self.schema_dn) + """
objectClass: top
objectClass: classSchema
adminDescription: """ + class_name + """
adminDisplayName: """ + class_name + """
cn: """ + class_name + """
governsId: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
schemaIdGuid: """ + str(uuid.uuid4()) + """
objectClassCategory: 1
subClassOf: organizationalPerson
systemMayContain: """ + attr_ldap_display_name + """
systemOnly: FALSE
"""
        self.ldb.add_ldif(ldif)

        # search for created objectclass
        res = []
        res = self.ldb.search("cn=%s,%s" % (class_name, self.schema_dn), scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["lDAPDisplayName"][0], class_ldap_display_name)
        self.assertEquals(res[0]["defaultObjectCategory"][0], res[0]["distinguishedName"][0])
        self.assertTrue("schemaIDGUID" in res[0])

        # store the class and the attribute
        self.dn_binary_class_ldap_display_name = class_ldap_display_name
        self.dn_binary_attribute = attr_ldap_display_name
        self.dn_binary_class_name = class_name

    def _get_object_ldif(self, object_name, class_name, class_ldap_display_name, attr_name, attr_value):
        # add object with correct syntax
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
""" + attr_name + attr_value  + """
"""
        return ldif

    def test_dn_string(self):
        # add obeject with correct value
        object_name1 = "obj-DN-String1" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name1, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCDE:" + self.base_dn)
        self.ldb.add_ldif(ldif)

        # search by specifying the DN part only
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=%s)" % (self.dn_string_attribute, self.base_dn))
        self.assertEquals(len(res), 0)

        # search by specifying the string part only
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=S:5:ABCDE)" % self.dn_string_attribute)
        self.assertEquals(len(res), 0)

        # search by DN+Stirng
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=S:5:ABCDE:%s)" % (self.dn_string_attribute, self.base_dn))
        self.assertEquals(len(res), 1)

        # add object with wrong format
        object_name2 = "obj-DN-String2" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name2, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCD:" + self.base_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_ATTRIBUTE_SYNTAX)

        # add object with the same dn but with different string value in case
        ldif = self._get_object_ldif(object_name1, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:abcde:" + self.base_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with the same dn but with different string value
        ldif = self._get_object_ldif(object_name1, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:FGHIJ:" + self.base_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with the same dn but with different dn and string value
        ldif = self._get_object_ldif(object_name1, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:FGHIJ:" + self.schema_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with the same dn but with different dn value
        ldif = self._get_object_ldif(object_name1, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCDE:" + self.schema_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with GUID instead of DN
        object_name3 = "obj-DN-String3" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name3, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCDE:<GUID=%s>" % str(uuid.uuid4()))
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # add object with SID instead of DN
        object_name4 = "obj-DN-String4" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name4, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCDE:<SID=%s>" % self.ldb.get_domain_sid())
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # add object with random string instead of DN
        object_name5 = "obj-DN-String5" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name5, self.dn_string_class_name, self.dn_string_class_ldap_display_name,
                               self.dn_string_attribute, ": S:5:ABCDE:randomSTRING")
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        pass

    def test_dn_binary(self):
        # add obeject with correct value
        object_name1 = "obj-DN-Binary1" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name1, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:1234:" + self.base_dn)
        self.ldb.add_ldif(ldif)

        # search by specifyingthe DN part
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=%s)" % (self.dn_binary_attribute, self.base_dn))
        self.assertEquals(len(res), 0)

        # search by specifying the binary part
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=B:4:1234)" % self.dn_binary_attribute)
        self.assertEquals(len(res), 0)

        # search by DN+Binary
        res = ldb.search(base=self.base_dn,
                         scope=SCOPE_SUBTREE,
                         expression="(%s=B:4:1234:%s)" % (self.dn_binary_attribute, self.base_dn))
        self.assertEquals(len(res), 1)

        # add object with wrong format - 5 bytes instead of 4, 8, 16, 32...
        object_name2 = "obj-DN-Binary2" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name2, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:5:67890:" + self.base_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_ATTRIBUTE_SYNTAX)

        # add object with the same dn but with different binary value
        ldif = self._get_object_ldif(object_name1, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:5678:" + self.base_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with the same dn but with different binary and dn value
        ldif = self._get_object_ldif(object_name1, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:5678:" + self.schema_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with the same dn but with different dn value
        ldif = self._get_object_ldif(object_name1, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:1234:" + self.schema_dn)
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # add object with GUID instead of DN
        object_name3 = "obj-DN-Binary3" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name3, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:1234:<GUID=%s>" % str(uuid.uuid4()))
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # add object with SID instead of DN
        object_name4 = "obj-DN-Binary4" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name4, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:1234:<SID=%s>" % self.ldb.get_domain_sid())
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # add object with random string instead of DN
        object_name5 = "obj-DN-Binary5" + time.strftime("%s", time.gmtime())
        ldif = self._get_object_ldif(object_name5, self.dn_binary_class_name, self.dn_binary_class_ldap_display_name,
                               self.dn_binary_attribute, ": B:4:1234:randomSTRING")
        try:
            self.ldb.add_ldif(ldif)
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        pass

ldb = samba.tests.connect_samdb(host, credentials=creds, session_info=system_session(lp), lp=lp)
runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(SyntaxTests)).wasSuccessful():
    rc = 1

sys.exit(rc)
