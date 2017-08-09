#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This is a port of the original in testprogs/ejs/ldap.js

import optparse
import sys
import time
import base64
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT, ERR_ATTRIBUTE_OR_VALUE_EXISTS
from ldb import ERR_ENTRY_ALREADY_EXISTS, ERR_UNWILLING_TO_PERFORM
from ldb import ERR_NOT_ALLOWED_ON_NON_LEAF, ERR_OTHER, ERR_INVALID_DN_SYNTAX
from ldb import ERR_NO_SUCH_ATTRIBUTE, ERR_INVALID_ATTRIBUTE_SYNTAX
from ldb import ERR_OBJECT_CLASS_VIOLATION, ERR_NOT_ALLOWED_ON_RDN
from ldb import ERR_NAMING_VIOLATION, ERR_CONSTRAINT_VIOLATION
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_ADD, FLAG_MOD_REPLACE, FLAG_MOD_DELETE
from ldb import timestring
from samba import Ldb
from samba.samdb import SamDB
from samba.dsdb import (UF_NORMAL_ACCOUNT,
    UF_WORKSTATION_TRUST_ACCOUNT,
    UF_PASSWD_NOTREQD, UF_ACCOUNTDISABLE, ATYPE_NORMAL_ACCOUNT,
    ATYPE_WORKSTATION_TRUST, SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE,
    SYSTEM_FLAG_CONFIG_ALLOW_RENAME, SYSTEM_FLAG_CONFIG_ALLOW_MOVE,
    SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE)

from subunit.run import SubunitTestRunner
import unittest

from samba.ndr import ndr_pack, ndr_unpack
from samba.dcerpc import security, lsa
from samba.tests import delete_force

parser = optparse.OptionParser("ldap.py [options] <host>")
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

    def setUp(self):
        super(BasicTests, self).setUp()
        self.ldb = ldb
        self.gc_ldb = gc_ldb
        self.base_dn = ldb.domain_dn()
        self.configuration_dn = ldb.get_config_basedn().get_linearized()
        self.schema_dn = ldb.get_schema_basedn().get_linearized()
        self.domain_sid = security.dom_sid(ldb.get_domain_sid())

        print "baseDN: %s\n" % self.base_dn

        delete_force(self.ldb, "cn=posixuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser3,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser4,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser4,cn=ldaptestcontainer2," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser5,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptest2computer,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestutf8user èùéìòà,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestutf8user2  èùéìòà,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer2," + self.base_dn)
        delete_force(self.ldb, "cn=parentguidtest,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=parentguidtest,cn=testotherusers," + self.base_dn)
        delete_force(self.ldb, "cn=testotherusers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestobject," + self.base_dn)
        delete_force(self.ldb, "description=xyz,cn=users," + self.base_dn)
        delete_force(self.ldb, "ou=testou,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=Test Secret,cn=system," + self.base_dn)

    def test_objectclasses(self):
        """Test objectClass behaviour"""
        print "Test objectClass behaviour"""

        # We cannot create LSA-specific objects (oc "secret" or "trustedDomain")
        try:
            self.ldb.add({
                "dn": "cn=Test Secret,cn=system," + self.base_dn,
                "objectClass": "secret" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Invalid objectclass specified
        try:
            self.ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectClass": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # Invalid objectclass specified
        try:
            self.ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectClass": "X" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        # Invalid objectCategory specified
        try:
            self.ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectClass": "person",
                "objectCategory": self.base_dn })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # Multi-valued "systemFlags"
        try:
            self.ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectClass": "person",
                "systemFlags": ["0", str(SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE)] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # We cannot instanciate from an abstract objectclass
        try:
            self.ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectClass": "connectionPoint" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Test allowed system flags
        self.ldb.add({
             "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
             "objectClass": "person",
             "systemFlags": str(~(SYSTEM_FLAG_CONFIG_ALLOW_RENAME | SYSTEM_FLAG_CONFIG_ALLOW_MOVE | SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE)) })

        res = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["systemFlags"])
        self.assertTrue(len(res) == 1)
        self.assertEquals(res[0]["systemFlags"][0], "0")

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        self.ldb.add({
             "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
             "objectClass": "person" })

        # We can remove derivation classes of the structural objectclass
        # but they're going to be readded afterwards
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("top", FLAG_MOD_DELETE,
          "objectClass")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["objectClass"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("top" in res[0]["objectClass"])

        # The top-most structural class cannot be deleted since there are
        # attributes of it in use
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("person", FLAG_MOD_DELETE,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # We cannot delete classes which weren't specified
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("computer", FLAG_MOD_DELETE,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        # An invalid class cannot be added
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("X", FLAG_MOD_ADD,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        # The top-most structural class cannot be changed by adding another
        # structural one
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("user", FLAG_MOD_ADD,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # An already specified objectclass cannot be added another time
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("person", FLAG_MOD_ADD,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        # Auxiliary classes can always be added
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("bootableDevice", FLAG_MOD_ADD,
          "objectClass")
        ldb.modify(m)

        # It's only possible to replace with the same objectclass combination.
        # So the replace action on "objectClass" attributes is really useless.
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement(["top", "person", "bootableDevice"],
          FLAG_MOD_REPLACE, "objectClass")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement(["person", "bootableDevice"],
          FLAG_MOD_REPLACE, "objectClass")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement(["top", "person", "bootableDevice",
          "connectionPoint"], FLAG_MOD_REPLACE, "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # More than one change operation is allowed
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m.add(MessageElement("bootableDevice", FLAG_MOD_DELETE, "objectClass"))
        m.add(MessageElement("bootableDevice", FLAG_MOD_ADD, "objectClass"))
        ldb.modify(m)

        # We cannot remove all object classes by an empty replace
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement([], FLAG_MOD_REPLACE, "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement(["top", "computer"], FLAG_MOD_REPLACE,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # Classes can be removed unless attributes of them are used.
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("bootableDevice", FLAG_MOD_DELETE,
          "objectClass")
        ldb.modify(m)

        res = ldb.search("cn=ldaptestuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["objectClass"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("bootableDevice" in res[0]["objectClass"])

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("bootableDevice", FLAG_MOD_ADD,
          "objectClass")
        ldb.modify(m)

        # Add an attribute specific to the "bootableDevice" class
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["bootParameter"] = MessageElement("test", FLAG_MOD_ADD,
          "bootParameter")
        ldb.modify(m)

        # Classes can be removed unless attributes of them are used. Now there
        # exist such attributes on the entry.
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("bootableDevice", FLAG_MOD_DELETE,
          "objectClass")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # Remove the previously specified attribute
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["bootParameter"] = MessageElement("test", FLAG_MOD_DELETE,
          "bootParameter")
        ldb.modify(m)

        # Classes can be removed unless attributes of them are used.
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("bootableDevice", FLAG_MOD_DELETE,
          "objectClass")
        ldb.modify(m)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

    def test_system_only(self):
        """Test systemOnly objects"""
        print "Test systemOnly objects"""

        try:
            self.ldb.add({
                "dn": "cn=ldaptestobject," + self.base_dn,
                "objectclass": "configuration"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            self.ldb.add({
                "dn": "cn=Test Secret,cn=system," + self.base_dn,
                "objectclass": "secret"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=ldaptestobject," + self.base_dn)
        delete_force(self.ldb, "cn=Test Secret,cn=system," + self.base_dn)

        # Create secret over LSA and try to change it

        lsa_conn = lsa.lsarpc("ncacn_np:%s" % args[0], lp, creds)
        lsa_handle = lsa_conn.OpenPolicy2(system_name="\\",
                                          attr=lsa.ObjectAttribute(),
                                          access_mask=security.SEC_FLAG_MAXIMUM_ALLOWED)
        secret_name = lsa.String()
        secret_name.string = "G$Test"
        sec_handle = lsa_conn.CreateSecret(handle=lsa_handle,
                                           name=secret_name,
                                           access_mask=security.SEC_FLAG_MAXIMUM_ALLOWED)
        lsa_conn.Close(lsa_handle)

        m = Message()
        m.dn = Dn(ldb, "cn=Test Secret,cn=system," + self.base_dn)
        m["description"] = MessageElement("desc", FLAG_MOD_REPLACE,
          "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=Test Secret,cn=system," + self.base_dn)

        try:
            self.ldb.add({
                "dn": "cn=ldaptestcontainer," + self.base_dn,
                "objectclass": "container",
                "isCriticalSystemObject": "TRUE"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.ldb.add({
            "dn": "cn=ldaptestcontainer," + self.base_dn,
            "objectclass": "container"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestcontainer," + self.base_dn)
        m["isCriticalSystemObject"] = MessageElement("TRUE", FLAG_MOD_REPLACE,
          "isCriticalSystemObject")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

        # Proof if DC SAM object has "isCriticalSystemObject" set
        res = self.ldb.search("", scope=SCOPE_BASE, attrs=["serverName"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("serverName" in res[0])
        res = self.ldb.search(res[0]["serverName"][0], scope=SCOPE_BASE,
                              attrs=["serverReference"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("serverReference" in res[0])
        res = self.ldb.search(res[0]["serverReference"][0], scope=SCOPE_BASE,
                              attrs=["isCriticalSystemObject"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("isCriticalSystemObject" in res[0])
        self.assertEquals(res[0]["isCriticalSystemObject"][0], "TRUE")

    def test_invalid_parent(self):
        """Test adding an object with invalid parent"""
        print "Test adding an object with invalid parent"""

        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=thisdoesnotexist123,"
                   + self.base_dn,
                "objectclass": "group"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=thisdoesnotexist123,"
          + self.base_dn)

        try:
            self.ldb.add({
                "dn": "ou=testou,cn=users," + self.base_dn,
                "objectclass": "organizationalUnit"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NAMING_VIOLATION)

        delete_force(self.ldb, "ou=testou,cn=users," + self.base_dn)

    def test_invalid_attribute(self):
        """Test invalid attributes on schema/objectclasses"""
        print "Test invalid attributes on schema/objectclasses"""

        # attributes not in schema test

        # add operation

        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "thisdoesnotexist": "x"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        self.ldb.add({
             "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
             "objectclass": "group"})

        # modify operation

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["thisdoesnotexist"] = MessageElement("x", FLAG_MOD_REPLACE,
          "thisdoesnotexist")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        # attributes not in objectclasses and mandatory attributes missing test
        # Use here a non-SAM entry since it doesn't have special triggers
        # associated which have an impact on the error results.

        # add operations

        # mandatory attribute missing
        try:
            self.ldb.add({
                "dn": "cn=ldaptestobject," + self.base_dn,
                "objectclass": "ipProtocol"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # inadequate but schema-valid attribute specified
        try:
            self.ldb.add({
                "dn": "cn=ldaptestobject," + self.base_dn,
                "objectclass": "ipProtocol",
                "ipProtocolNumber": "1",
                "uid" : "0"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        self.ldb.add({
            "dn": "cn=ldaptestobject," + self.base_dn,
            "objectclass": "ipProtocol",
            "ipProtocolNumber": "1"})

        # modify operations

        # inadequate but schema-valid attribute add trial
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestobject," + self.base_dn)
        m["uid"] = MessageElement("0", FLAG_MOD_ADD, "uid")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # mandatory attribute delete trial
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestobject," + self.base_dn)
        m["ipProtocolNumber"] = MessageElement([], FLAG_MOD_DELETE,
          "ipProtocolNumber")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        # mandatory attribute delete trial
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestobject," + self.base_dn)
        m["ipProtocolNumber"] = MessageElement([], FLAG_MOD_REPLACE,
          "ipProtocolNumber")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        delete_force(self.ldb, "cn=ldaptestobject," + self.base_dn)

    def test_single_valued_attributes(self):
        """Test single-valued attributes"""
        print "Test single-valued attributes"""

        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "sAMAccountName": ["nam1", "nam2"]})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        self.ldb.add({
             "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
             "objectclass": "group"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement(["nam1","nam2"], FLAG_MOD_REPLACE,
          "sAMAccountName")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testgroupXX", FLAG_MOD_REPLACE,
          "sAMAccountName")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["sAMAccountName"] = MessageElement("testgroupXX2", FLAG_MOD_ADD,
          "sAMAccountName")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_attribute_ranges(self):
        """Test attribute ranges"""
        print "Test attribute ranges"""

        # Too short (min. 1)
        try:
            ldb.add({
               "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
               "objectClass": "person",
               "sn": "" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_ATTRIBUTE_SYNTAX)

        # Too long (max. 64)
#        try:
#            ldb.add({
#               "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
#               "objectClass": "person",
#               "sn": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" })
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        ldb.add({
           "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
           "objectClass": "person" })

        # Too short (min. 1)
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sn"] = MessageElement("", FLAG_MOD_REPLACE, "sn")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_ATTRIBUTE_SYNTAX)

        # Too long (max. 64)
#        m = Message()
#        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
#        m["sn"] = MessageElement("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", FLAG_MOD_REPLACE, "sn")
#        try:
#            ldb.modify(m)
#            self.fail()
#        except LdbError, (num, _):
#            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["sn"] = MessageElement("x", FLAG_MOD_REPLACE, "sn")
        ldb.modify(m)

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

    def test_empty_messages(self):
        """Test empty messages"""
        print "Test empty messages"""

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        try:
            ldb.add(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OBJECT_CLASS_VIOLATION)

        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_empty_attributes(self):
        """Test empty attributes"""
        print "Test empty attributes"""

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("group", FLAG_MOD_ADD, "objectClass")
        m["description"] = MessageElement([], FLAG_MOD_ADD, "description")

        try:
            ldb.add(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        self.ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement([], FLAG_MOD_ADD, "description")

        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement([], FLAG_MOD_REPLACE, "description")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["description"] = MessageElement([], FLAG_MOD_DELETE, "description")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_instanceType(self):
        """Tests the 'instanceType' attribute"""
        print "Tests the 'instanceType' attribute"""

        # The instance type is single-valued
        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "instanceType": ["0", "1"]})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # The head NC flag cannot be set without the write flag
        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "instanceType": "1" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # We cannot manipulate NCs without the head NC flag
        try:
            self.ldb.add({
                "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
                "objectclass": "group",
                "instanceType": "32" })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.ldb.add({
             "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
             "objectclass": "group"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["instanceType"] = MessageElement("0", FLAG_MOD_REPLACE,
          "instanceType")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["instanceType"] = MessageElement([], FLAG_MOD_REPLACE,
          "instanceType")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["instanceType"] = MessageElement([], FLAG_MOD_DELETE, "instanceType")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_distinguished_name(self):
        """Tests the 'distinguishedName' attribute"""
        print "Tests the 'distinguishedName' attribute"""

        # The "dn" shortcut isn't supported
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["objectClass"] = MessageElement("group", 0, "objectClass")
        m["dn"] = MessageElement("cn=ldaptestgroup,cn=users," + self.base_dn, 0,
          "dn")
        try:
            ldb.add(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        # a wrong "distinguishedName" attribute is obviously tolerated
        self.ldb.add({
              "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
              "objectclass": "group",
              "distinguishedName": "cn=ldaptest,cn=users," + self.base_dn})

        # proof if the DN has been set correctly
        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["distinguishedName"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("distinguishedName" in res[0])
        self.assertTrue(Dn(ldb, res[0]["distinguishedName"][0])
           == Dn(ldb, "cn=ldaptestgroup, cn=users," + self.base_dn))

        # The "dn" shortcut isn't supported
        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["dn"] = MessageElement(
          "cn=ldaptestgroup,cn=users," + self.base_dn, FLAG_MOD_REPLACE,
          "dn")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["distinguishedName"] = MessageElement(
          "cn=ldaptestuser,cn=users," + self.base_dn, FLAG_MOD_ADD,
          "distinguishedName")

        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["distinguishedName"] = MessageElement(
          "cn=ldaptestuser,cn=users," + self.base_dn, FLAG_MOD_REPLACE,
          "distinguishedName")

        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["distinguishedName"] = MessageElement(
          "cn=ldaptestuser,cn=users," + self.base_dn, FLAG_MOD_DELETE,
          "distinguishedName")

        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_rdn_name(self):
        """Tests the RDN"""
        print "Tests the RDN"""

        # Search

        # empty RDN
        try:
            self.ldb.search("=,cn=users," + self.base_dn, scope=SCOPE_BASE)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # empty RDN name
        try:
            self.ldb.search("cn=,cn=users," + self.base_dn, scope=SCOPE_BASE)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        try:
            self.ldb.search("=ldaptestgroup,cn=users," + self.base_dn, scope=SCOPE_BASE)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # Add

        # empty RDN
        try:
            self.ldb.add({
                 "dn": "=,cn=users," + self.base_dn,
                 "objectclass": "group"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # empty RDN name
        try:
            self.ldb.add({
                 "dn": "=ldaptestgroup,cn=users," + self.base_dn,
                 "objectclass": "group"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # empty RDN value
        try:
            self.ldb.add({
                 "dn": "cn=,cn=users," + self.base_dn,
                 "objectclass": "group"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # a wrong RDN candidate
        try:
            self.ldb.add({
                 "dn": "description=xyz,cn=users," + self.base_dn,
                 "objectclass": "group"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NAMING_VIOLATION)

        delete_force(self.ldb, "description=xyz,cn=users," + self.base_dn)

        # a wrong "name" attribute is obviously tolerated
        self.ldb.add({
             "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
             "objectclass": "group",
             "name": "ldaptestgroupx"})

        # proof if the name has been set correctly
        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["name"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("name" in res[0])
        self.assertTrue(res[0]["name"][0] == "ldaptestgroup")

        # Modify

        # empty RDN value
        m = Message()
        m.dn = Dn(ldb, "cn=,cn=users," + self.base_dn)
        m["description"] = "test"
        try:
            self.ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # Delete

        # empty RDN value
        try:
            self.ldb.delete("cn=,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # Rename

        # new empty RDN
        try:
            self.ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn,
                            "=,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # new empty RDN name
        try:
            self.ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn,
                            "=ldaptestgroup,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # new empty RDN value
        try:
            self.ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn,
                            "cn=,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NAMING_VIOLATION)

        # new wrong RDN candidate
        try:
            self.ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn,
                            "description=xyz,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "description=xyz,cn=users," + self.base_dn)

        # old empty RDN value
        try:
            self.ldb.rename("cn=,cn=users," + self.base_dn,
                            "cn=ldaptestgroup,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        # names

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["name"] = MessageElement("cn=ldaptestuser", FLAG_MOD_REPLACE,
          "name")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_RDN)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["cn"] = MessageElement("ldaptestuser",
          FLAG_MOD_REPLACE, "cn")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NOT_ALLOWED_ON_RDN)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)


        # this test needs to be disabled until we really understand
        # what the rDN length constraints are
    def DISABLED_test_largeRDN(self):
        """Testing large rDN (limit 64 characters)"""
        rdn = "CN=a012345678901234567890123456789012345678901234567890123456789012";
        delete_force(self.ldb, "%s,%s" % (rdn, self.base_dn))
        ldif = """
dn: %s,%s""" % (rdn,self.base_dn) + """
objectClass: container
"""
        self.ldb.add_ldif(ldif)
        delete_force(self.ldb, "%s,%s" % (rdn, self.base_dn))

        rdn = "CN=a0123456789012345678901234567890123456789012345678901234567890120";
        delete_force(self.ldb, "%s,%s" % (rdn, self.base_dn))
        try:
            ldif = """
dn: %s,%s""" % (rdn,self.base_dn) + """
objectClass: container
"""
            self.ldb.add_ldif(ldif)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        delete_force(self.ldb, "%s,%s" % (rdn, self.base_dn))

    def test_rename(self):
        """Tests the rename operation"""
        print "Tests the rename operations"""

        try:
            # cannot rename to be a child of itself
            ldb.rename(self.base_dn, "dc=test," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            # inexistent object
            ldb.rename("cn=ldaptestuser2,cn=users," + self.base_dn, "cn=ldaptestuser2,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        self.ldb.add({
             "dn": "cn=ldaptestuser2,cn=users," + self.base_dn,
             "objectclass": "user" })

        ldb.rename("cn=ldaptestuser2,cn=users," + self.base_dn, "cn=ldaptestuser2,cn=users," + self.base_dn)
        ldb.rename("cn=ldaptestuser2,cn=users," + self.base_dn, "cn=ldaptestuser3,cn=users," + self.base_dn)
        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestUSER3,cn=users," + self.base_dn)

        try:
            # containment problem: a user entry cannot contain user entries
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser4,cn=ldaptestuser3,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NAMING_VIOLATION)

        try:
            # invalid parent
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser3,cn=people,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)

        try:
            # invalid target DN syntax
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, ",cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        try:
            # invalid RDN name
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "ou=ldaptestuser3,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, "cn=ldaptestuser3,cn=users," + self.base_dn)

        # Performs some "systemFlags" testing

        # Move failing since no "SYSTEM_FLAG_CONFIG_ALLOW_MOVE"
        try:
            ldb.rename("CN=DisplaySpecifiers," + self.configuration_dn, "CN=DisplaySpecifiers,CN=Services," + self.configuration_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Limited move failing since no "SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE"
        try:
            ldb.rename("CN=Directory Service,CN=Windows NT,CN=Services," + self.configuration_dn, "CN=Directory Service,CN=RRAS,CN=Services," + self.configuration_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Rename failing since no "SYSTEM_FLAG_CONFIG_ALLOW_RENAME"
        try:
            ldb.rename("CN=DisplaySpecifiers," + self.configuration_dn, "CN=DisplaySpecifiers2," + self.configuration_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # It's not really possible to test moves on the schema partition since
        # there don't exist subcontainers on it.

        # Rename failing since "SYSTEM_FLAG_SCHEMA_BASE_OBJECT"
        try:
            ldb.rename("CN=Top," + self.schema_dn, "CN=Top2," + self.schema_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Move failing since "SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE"
        try:
            ldb.rename("CN=Users," + self.base_dn, "CN=Users,CN=Computers," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Rename failing since "SYSTEM_FLAG_DOMAIN_DISALLOW_RENAME"
        try:
            ldb.rename("CN=Users," + self.base_dn, "CN=Users2," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        # Performs some other constraints testing

        try:
            ldb.rename("CN=Policies,CN=System," + self.base_dn, "CN=Users2," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_OTHER)

    def test_rename_twice(self):
        """Tests the rename operation twice - this corresponds to a past bug"""
        print "Tests the rename twice operation"""

        self.ldb.add({
             "dn": "cn=ldaptestuser5,cn=users," + self.base_dn,
             "objectclass": "user" })

        ldb.rename("cn=ldaptestuser5,cn=users," + self.base_dn, "cn=ldaptestUSER5,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser5,cn=users," + self.base_dn)
        self.ldb.add({
             "dn": "cn=ldaptestuser5,cn=users," + self.base_dn,
             "objectclass": "user" })
        ldb.rename("cn=ldaptestuser5,cn=Users," + self.base_dn, "cn=ldaptestUSER5,cn=users," + self.base_dn)
        res = ldb.search(expression="cn=ldaptestuser5")
        print "Found %u records" % len(res)
        self.assertEquals(len(res), 1, "Wrong number of hits for cn=ldaptestuser5")
        res = ldb.search(expression="(&(cn=ldaptestuser5)(objectclass=user))")
        print "Found %u records" % len(res)
        self.assertEquals(len(res), 1, "Wrong number of hits for (&(cn=ldaptestuser5)(objectclass=user))")
        delete_force(self.ldb, "cn=ldaptestuser5,cn=users," + self.base_dn)

    def test_objectGUID(self):
        """Test objectGUID behaviour"""
        print "Testing objectGUID behaviour\n"

        # The objectGUID cannot directly be set
        try:
            self.ldb.add_ldif("""
dn: cn=ldaptestcontainer,""" + self.base_dn + """
objectClass: container
objectGUID: bd3480c9-58af-4cd8-92df-bc4a18b6e44d
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        self.ldb.add({
            "dn": "cn=ldaptestcontainer," + self.base_dn,
            "objectClass": "container" })

        res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                         scope=SCOPE_BASE,
                         attrs=["objectGUID", "uSNCreated", "uSNChanged", "whenCreated", "whenChanged"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("uSNCreated" in res[0])
        self.assertTrue("uSNChanged" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertTrue("whenChanged" in res[0])

        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

        # All the following attributes are specificable on add operations
        self.ldb.add({
            "dn": "cn=ldaptestcontainer," + self.base_dn,
            "objectClass": "container",
            "uSNCreated" : "1",
            "uSNChanged" : "1",
            "whenCreated": timestring(long(time.time())),
            "whenChanged": timestring(long(time.time())) })

        res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                         scope=SCOPE_BASE,
                         attrs=["objectGUID", "uSNCreated", "uSNChanged", "whenCreated", "whenChanged"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("uSNCreated" in res[0])
        self.assertFalse(res[0]["uSNCreated"][0] == "1") # these are corrected
        self.assertTrue("uSNChanged" in res[0])
        self.assertFalse(res[0]["uSNChanged"][0] == "1") # these are corrected

        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

        # All this attributes are specificable on add operations
        self.ldb.add({
            "dn": "cn=ldaptestcontainer," + self.base_dn,
            "objectclass": "container",
            "uSNCreated" : "1",
            "uSNChanged" : "1",
            "whenCreated": timestring(long(time.time())),
            "whenChanged": timestring(long(time.time())) })

        res = ldb.search("cn=ldaptestcontainer," + self.base_dn,
                         scope=SCOPE_BASE,
                         attrs=["objectGUID", "uSNCreated", "uSNChanged", "whenCreated", "whenChanged"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("objectGUID" in res[0])
        self.assertTrue("uSNCreated" in res[0])
        self.assertFalse(res[0]["uSNCreated"][0] == "1") # these are corrected
        self.assertTrue("uSNChanged" in res[0])
        self.assertFalse(res[0]["uSNChanged"][0] == "1") # these are corrected
        self.assertTrue("whenCreated" in res[0])
        self.assertTrue("whenChanged" in res[0])

        # The objectGUID cannot directly be changed
        try:
            self.ldb.modify_ldif("""
dn: cn=ldaptestcontainer,""" + self.base_dn + """
changetype: modify
replace: objectGUID
objectGUID: bd3480c9-58af-4cd8-92df-bc4a18b6e44d
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)

    def test_parentGUID(self):
        """Test parentGUID behaviour"""
        print "Testing parentGUID behaviour\n"

        self.ldb.add({
            "dn": "cn=parentguidtest,cn=users," + self.base_dn,
            "objectclass":"user",
            "samaccountname":"parentguidtest"});
        res1 = ldb.search(base="cn=parentguidtest,cn=users," + self.base_dn, scope=SCOPE_BASE,
                          attrs=["parentGUID", "samaccountname"]);
        res2 = ldb.search(base="cn=users," + self.base_dn,scope=SCOPE_BASE,
                          attrs=["objectGUID"]);
        res3 = ldb.search(base=self.base_dn, scope=SCOPE_BASE,
                          attrs=["parentGUID"]);
        res4 = ldb.search(base=self.configuration_dn, scope=SCOPE_BASE,
                          attrs=["parentGUID"]);
        res5 = ldb.search(base=self.schema_dn, scope=SCOPE_BASE,
                          attrs=["parentGUID"]);

        """Check if the parentGUID is valid """
        self.assertEquals(res1[0]["parentGUID"], res2[0]["objectGUID"]);

        """Check if it returns nothing when there is no parent object - default NC"""
        has_parentGUID = False
        for key in res3[0].keys():
            if key == "parentGUID":
                has_parentGUID = True
                break
        self.assertFalse(has_parentGUID);

        """Check if it returns nothing when there is no parent object - configuration NC"""
        has_parentGUID = False
        for key in res4[0].keys():
            if key == "parentGUID":
                has_parentGUID = True
                break
        self.assertFalse(has_parentGUID);

        """Check if it returns nothing when there is no parent object - schema NC"""
        has_parentGUID = False
        for key in res5[0].keys():
            if key == "parentGUID":
                has_parentGUID = True
                break
        self.assertFalse(has_parentGUID);

        """Ensures that if you look for another object attribute after the constructed
            parentGUID, it will return correctly"""
        has_another_attribute = False
        for key in res1[0].keys():
            if key == "sAMAccountName":
                has_another_attribute = True
                break
        self.assertTrue(has_another_attribute)
        self.assertTrue(len(res1[0]["samaccountname"]) == 1)
        self.assertEquals(res1[0]["samaccountname"][0], "parentguidtest");

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

        delete_force(self.ldb, "cn=parentguidtest,cn=testotherusers," + self.base_dn)
        delete_force(self.ldb, "cn=testotherusers," + self.base_dn)

    def test_groupType_int32(self):
        """Test groupType (int32) behaviour (should appear to be casted to a 32 bit signed integer before comparsion)"""
        print "Testing groupType (int32) behaviour\n"

        res1 = ldb.search(base=self.base_dn, scope=SCOPE_SUBTREE,
                          attrs=["groupType"], expression="groupType=2147483653");

        res2 = ldb.search(base=self.base_dn, scope=SCOPE_SUBTREE,
                          attrs=["groupType"], expression="groupType=-2147483643");

        self.assertEquals(len(res1), len(res2))

        self.assertTrue(res1.count > 0)

        self.assertEquals(res1[0]["groupType"][0], "-2147483643")

    def test_linked_attributes(self):
        """This tests the linked attribute behaviour"""
        print "Testing linked attribute behaviour\n"

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group"})

        # This should not work since "memberOf" is linked to "member"
        try:
            ldb.add({
                "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
                "objectclass": "user",
                "memberOf": "cn=ldaptestgroup,cn=users," + self.base_dn})
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        ldb.add({
            "dn": "cn=ldaptestuser,cn=users," + self.base_dn,
            "objectclass": "user"})

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["memberOf"] = MessageElement("cn=ldaptestgroup,cn=users," + self.base_dn,
          FLAG_MOD_ADD, "memberOf")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
          FLAG_MOD_ADD, "member")
        ldb.modify(m)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["memberOf"] = MessageElement("cn=ldaptestgroup,cn=users," + self.base_dn,
          FLAG_MOD_REPLACE, "memberOf")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        m["memberOf"] = MessageElement("cn=ldaptestgroup,cn=users," + self.base_dn,
          FLAG_MOD_DELETE, "memberOf")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        m["member"] = MessageElement("cn=ldaptestuser,cn=users," + self.base_dn,
          FLAG_MOD_DELETE, "member")
        ldb.modify(m)

        # This should yield no results since the member attribute for
        # "ldaptestuser" should have been deleted
        res1 = ldb.search("cn=ldaptestgroup, cn=users," + self.base_dn,
                          scope=SCOPE_BASE,
                          expression="(member=cn=ldaptestuser,cn=users," + self.base_dn + ")",
                          attrs=[])
        self.assertTrue(len(res1) == 0)

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=users," + self.base_dn,
            "objectclass": "group",
            "member": "cn=ldaptestuser,cn=users," + self.base_dn})

        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)

        # Make sure that the "member" attribute for "ldaptestuser" has been
        # removed
        res = ldb.search("cn=ldaptestgroup,cn=users," + self.base_dn,
                          scope=SCOPE_BASE, attrs=["member"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("member" in res[0])

        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)

    def test_wkguid(self):
        """Test Well known GUID behaviours (including DN+Binary)"""
        print "Test Well known GUID behaviours (including DN+Binary)"""

        res = self.ldb.search(base=("<WKGUID=ab1d30f3768811d1aded00c04fd8d5cd,%s>" % self.base_dn), scope=SCOPE_BASE, attrs=[])
        self.assertEquals(len(res), 1)

        res2 = self.ldb.search(scope=SCOPE_BASE, attrs=["wellKnownObjects"], expression=("wellKnownObjects=B:32:ab1d30f3768811d1aded00c04fd8d5cd:%s" % res[0].dn))
        self.assertEquals(len(res2), 1)

        # Prove that the matching rule is over the whole DN+Binary
        res2 = self.ldb.search(scope=SCOPE_BASE, attrs=["wellKnownObjects"], expression=("wellKnownObjects=B:32:ab1d30f3768811d1aded00c04fd8d5cd"))
        self.assertEquals(len(res2), 0)
        # Prove that the matching rule is over the whole DN+Binary
        res2 = self.ldb.search(scope=SCOPE_BASE, attrs=["wellKnownObjects"], expression=("wellKnownObjects=%s") % res[0].dn)
        self.assertEquals(len(res2), 0)

    def test_subschemasubentry(self):
        """Test subSchemaSubEntry appears when requested, but not when not requested"""
        print "Test subSchemaSubEntry"""

        res = self.ldb.search(base=self.base_dn, scope=SCOPE_BASE, attrs=["subSchemaSubEntry"])
        self.assertEquals(len(res), 1)
        self.assertEquals(res[0]["subSchemaSubEntry"][0], "CN=Aggregate,"+self.schema_dn)

        res = self.ldb.search(base=self.base_dn, scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.assertTrue("subScheamSubEntry" not in res[0])

    def test_all(self):
        """Basic tests"""

        print "Testing user add"

        ldb.add({
            "dn": "cn=ldaptestuser,cn=uSers," + self.base_dn,
            "objectclass": "user",
            "cN": "LDAPtestUSER",
            "givenname": "ldap",
            "sn": "testy"})

        ldb.add({
            "dn": "cn=ldaptestgroup,cn=uSers," + self.base_dn,
            "objectclass": "group",
            "member": "cn=ldaptestuser,cn=useRs," + self.base_dn})

        ldb.add({
            "dn": "cn=ldaptestcomputer,cn=computers," + self.base_dn,
            "objectclass": "computer",
            "cN": "LDAPtestCOMPUTER"})

        ldb.add({"dn": "cn=ldaptest2computer,cn=computers," + self.base_dn,
            "objectClass": "computer",
            "cn": "LDAPtest2COMPUTER",
            "userAccountControl": str(UF_WORKSTATION_TRUST_ACCOUNT),
            "displayname": "ldap testy"})

        try:
            ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                     "objectClass": "computer",
                     "cn": "LDAPtest2COMPUTER"
                     })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INVALID_DN_SYNTAX)

        try:
            ldb.add({"dn": "cn=ldaptestcomputer3,cn=computers," + self.base_dn,
                     "objectClass": "computer",
                     "cn": "ldaptestcomputer3",
                     "sAMAccountType": str(ATYPE_NORMAL_ACCOUNT)
                })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

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
        self.assertEquals(int(res[0]["sAMAccountType"][0]), ATYPE_NORMAL_ACCOUNT);
        self.assertEquals(int(res[0]["userAccountControl"][0]), UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD | UF_ACCOUNTDISABLE);

        delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)

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

        delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        ldb.add({
            "dn": "cn=ldaptestuser2,cn=useRs," + self.base_dn,
            "objectClass": "user",
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

#        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
#        self.assertEquals(res[0]["cn"][0], "ldaptestuser")
#        self.assertEquals(res[0]["name"][0], "ldaptestuser")

        # Testing ldb.search for (&(anr==testy ldap)(objectClass=user))
#        res = ldb.search(expression="(&(anr==testy ldap)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Could not find (&(anr==testy ldap)(objectClass=user))")

#        self.assertEquals(str(res[0].dn), ("CN=ldaptestuser,CN=Users," + self.base_dn))
#        self.assertEquals(res[0]["cn"][0], "ldaptestuser")
#        self.assertEquals(res[0]["name"][0], "ldaptestuser")

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

        print "Testing Renames"

        attrs = ["objectGUID", "objectSid"]
        print "Testing ldb.search for (&(cn=ldaptestUSer2)(objectClass=user))"
        res_user = ldb.search(self.base_dn, expression="(&(cn=ldaptestUSer2)(objectClass=user))", scope=SCOPE_SUBTREE, attrs=attrs)
        self.assertEquals(len(res_user), 1, "Could not find (&(cn=ldaptestUSer2)(objectClass=user))")

        # Check rename works with extended/alternate DN forms
        ldb.rename("<SID=" + ldb.schema_format_value("objectSID", res_user[0]["objectSID"][0]) + ">" , "cn=ldaptestUSER3,cn=users," + self.base_dn)

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
                      "objectClass": "user",
                      "cn": "LDAPtestUSER3"})
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ENTRY_ALREADY_EXISTS)

        # rename back
        ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn, "cn=ldaptestuser2,cn=users," + self.base_dn)

        # ensure we cannot rename it twice
        try:
            ldb.rename("cn=ldaptestuser3,cn=users," + self.base_dn,
                       "cn=ldaptestuser2,cn=users," + self.base_dn)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_OBJECT)

        # ensure can now use that name
        ldb.add({"dn": "cn=ldaptestuser3,cn=users," + self.base_dn,
                      "objectClass": "user",
                      "cn": "LDAPtestUSER3"})

        # ensure we now cannot rename
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

        delete_force(ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)

        ldb.rename("cn=ldaptestgroup,cn=users," + self.base_dn, "cn=ldaptestgroup2,cn=users," + self.base_dn)

        print "Testing subtree renames"

        ldb.add({"dn": "cn=ldaptestcontainer," + self.base_dn,
                 "objectClass": "container"})

        ldb.add({"dn": "CN=ldaptestuser4,CN=ldaptestcontainer," + self.base_dn,
                 "objectClass": "user",
                 "cn": "LDAPtestUSER4"})

        # Here we don't enforce these hard "description" constraints
        ldb.modify_ldif("""
dn: cn=ldaptestcontainer,""" + self.base_dn + """
changetype: modify
replace: description
description: desc1
description: desc2
""")

        ldb.modify_ldif("""
dn: cn=ldaptestgroup2,cn=users,""" + self.base_dn + """
changetype: modify
add: member
member: cn=ldaptestuser4,cn=ldaptestcontainer,""" + self.base_dn + """
member: cn=ldaptestcomputer,cn=computers,""" + self.base_dn + """
member: cn=ldaptestuser2,cn=users,""" + self.base_dn + """
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

        ldb.add({"dn": "cn=ldaptestutf8user èùéìòà,cn=users," + self.base_dn, "objectClass": "user"})

        ldb.add({"dn": "cn=ldaptestutf8user2  èùéìòà,cn=users," + self.base_dn, "objectClass": "user"})

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
        self.assertEquals(int(res[0]["sAMAccountType"][0]), ATYPE_NORMAL_ACCOUNT)
        self.assertEquals(int(res[0]["userAccountControl"][0]), UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD | UF_ACCOUNTDISABLE)
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

        if gc_ldb is not None:
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
        self.assertEquals(int(res[0]["sAMAccountType"][0]), ATYPE_NORMAL_ACCOUNT)
        self.assertEquals(int(res[0]["userAccountControl"][0]), UF_NORMAL_ACCOUNT | UF_PASSWD_NOTREQD | UF_ACCOUNTDISABLE)
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
        self.assertEquals(int(res[0]["sAMAccountType"][0]), ATYPE_WORKSTATION_TRUST)
        self.assertEquals(int(res[0]["userAccountControl"][0]), UF_WORKSTATION_TRUST_ACCOUNT)

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

        print "Quicktest for linked attributes"
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
# TODO UTF8 users don't seem to work fully anymore
#        res = ldb.search(expression="(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")
        res = ldb.search(expression="(&(cn=ldaptestutf8user èùéìòà)(objectclass=user))")
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
# TODO UTF8 users don't seem to work fully anymore
#        res = ldb.search(expression="(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")
#        self.assertEquals(len(res), 1, "Could not find (&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))")

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

        print "Testing creating a user with the posixAccount objectClass"
        self.ldb.add_ldif("""dn: cn=posixuser,CN=Users,%s
objectClass: top
objectClass: person
objectClass: posixAccount
objectClass: user
objectClass: organizationalPerson
cn: posixuser
uid: posixuser
sn: posixuser
uidNumber: 10126
gidNumber: 10126
homeDirectory: /home/posixuser
loginShell: /bin/bash
gecos: Posix User;;;
description: A POSIX user"""% (self.base_dn))

        print "Testing removing the posixAccount objectClass from an existing user"
        self.ldb.modify_ldif("""dn: cn=posixuser,CN=Users,%s
changetype: modify
delete: objectClass
objectClass: posixAccount"""% (self.base_dn))

        print "Testing adding the posixAccount objectClass to an existing user"
        self.ldb.modify_ldif("""dn: cn=posixuser,CN=Users,%s
changetype: modify
add: objectClass
objectClass: posixAccount"""% (self.base_dn))

        delete_force(self.ldb, "cn=posixuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser3,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser4,cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser4,cn=ldaptestcontainer2," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestuser5,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestgroup2,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptest2computer,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcomputer3,cn=computers," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestutf8user èùéìòà,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestutf8user2  èùéìòà,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer," + self.base_dn)
        delete_force(self.ldb, "cn=ldaptestcontainer2," + self.base_dn)

    def test_security_descriptor_add(self):
        """ Testing ldb.add_ldif() for nTSecurityDescriptor """
        user_name = "testdescriptoruser1"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        #
        # Test an empty security descriptor (naturally this shouldn't work)
        #
        delete_force(self.ldb, user_dn)
        try:
            self.ldb.add({ "dn": user_dn,
                           "objectClass": "user",
                           "sAMAccountName": user_name,
                           "nTSecurityDescriptor": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        finally:
            delete_force(self.ldb, user_dn)
        #
        # Test add_ldif() with SDDL security descriptor input
        #
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
            delete_force(self.ldb, user_dn)
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
            delete_force(self.ldb, user_dn)

    def test_security_descriptor_add_neg(self):
        """Test add_ldif() with BASE64 security descriptor input using WRONG domain SID
            Negative test
        """
        user_name = "testdescriptoruser1"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        delete_force(self.ldb, user_dn)
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
            self.assertTrue("nTSecurityDescriptor" in res[0])
        finally:
            delete_force(self.ldb, user_dn)

    def test_security_descriptor_modify(self):
        """ Testing ldb.modify_ldif() for nTSecurityDescriptor """
        user_name = "testdescriptoruser2"
        user_dn = "CN=%s,CN=Users,%s" % (user_name, self.base_dn)
        #
        # Test an empty security descriptor (naturally this shouldn't work)
        #
        delete_force(self.ldb, user_dn)
        self.ldb.add({ "dn": user_dn,
                       "objectClass": "user",
                       "sAMAccountName": user_name })

        m = Message()
        m.dn = Dn(ldb, user_dn)
        m["nTSecurityDescriptor"] = MessageElement([], FLAG_MOD_ADD,
                                                   "nTSecurityDescriptor")
        try:
            self.ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, user_dn)
        m["nTSecurityDescriptor"] = MessageElement([], FLAG_MOD_REPLACE,
                                                   "nTSecurityDescriptor")
        try:
            self.ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, user_dn)
        m["nTSecurityDescriptor"] = MessageElement([], FLAG_MOD_DELETE,
                                                   "nTSecurityDescriptor")
        try:
            self.ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        delete_force(self.ldb, user_dn)
        #
        # Test modify_ldif() with SDDL security descriptor input
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
            delete_force(self.ldb, user_dn)
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
            delete_force(self.ldb, user_dn)
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
            delete_force(self.ldb, user_dn)
        #
        # Test modify_ldif() with BASE64 security descriptor input
        # New descriptor test
        #
        try:
            delete_force(self.ldb, user_dn)
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
            delete_force(self.ldb, user_dn)

    def test_dsheuristics(self):
        """Tests the 'dSHeuristics' attribute"""
        print "Tests the 'dSHeuristics' attribute"""

        # Get the current value to restore it later
        dsheuristics = self.ldb.get_dsheuristics()
        # Should not be longer than 18 chars?
        try:
            self.ldb.set_dsheuristics("123ABC-+!1asdfg@#^12")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        # If it is >= 10 chars, tenthChar should be 1
        try:
            self.ldb.set_dsheuristics("00020000000002")
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
        # apart from the above, all char values are accepted
        self.ldb.set_dsheuristics("123ABC-+!1asdfg@#^")
        self.assertEquals(self.ldb.get_dsheuristics(), "123ABC-+!1asdfg@#^")
        # restore old value
        self.ldb.set_dsheuristics(dsheuristics)

    def test_ldapControlReturn(self):
        """Testing that if we request a control that return a control it
           really return something"""
        res = self.ldb.search(attrs=["cn"],
                              controls=["paged_result:1:10"])
        self.assertEquals(len(res.controls), 1)
        self.assertEquals(res.controls[0].oid, "1.2.840.113556.1.4.319")

    def test_operational(self):
        """Tests operational attributes"""
        print "Tests operational attributes"""

        res = self.ldb.search(self.base_dn, scope=SCOPE_BASE,
                              attrs=["createTimeStamp", "modifyTimeStamp",
                                     "structuralObjectClass", "whenCreated",
                                     "whenChanged"])
        self.assertEquals(len(res), 1)
        self.assertTrue("createTimeStamp" in res[0])
        self.assertTrue("modifyTimeStamp" in res[0])
        self.assertTrue("structuralObjectClass" in res[0])
        self.assertTrue("whenCreated" in res[0])
        self.assertTrue("whenChanged" in res[0])

class BaseDnTests(unittest.TestCase):

    def setUp(self):
        super(BaseDnTests, self).setUp()
        self.ldb = ldb

    def test_rootdse_attrs(self):
        """Testing for all rootDSE attributes"""
        res = self.ldb.search("", scope=SCOPE_BASE, attrs=[])
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

    def test_namingContexts(self):
        """Testing for namingContexts in rootDSE"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                attrs=["namingContexts", "defaultNamingContext", "schemaNamingContext", "configurationNamingContext"])
        self.assertEquals(len(res), 1)

        ncs = set([])
        for nc in res[0]["namingContexts"]:
            self.assertTrue(nc not in ncs)
            ncs.add(nc)

        self.assertTrue(res[0]["defaultNamingContext"][0] in ncs)
        self.assertTrue(res[0]["configurationNamingContext"][0] in ncs)
        self.assertTrue(res[0]["schemaNamingContext"][0] in ncs)

    def test_serverPath(self):
        """Testing the server paths in rootDSE"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                              attrs=["dsServiceName", "serverName"])
        self.assertEquals(len(res), 1)

        self.assertTrue("CN=Servers" in res[0]["dsServiceName"][0])
        self.assertTrue("CN=Sites" in res[0]["dsServiceName"][0])
        self.assertTrue("CN=NTDS Settings" in res[0]["dsServiceName"][0])
        self.assertTrue("CN=Servers" in res[0]["serverName"][0])
        self.assertTrue("CN=Sites" in res[0]["serverName"][0])
        self.assertFalse("CN=NTDS Settings" in res[0]["serverName"][0])

    def test_functionality(self):
        """Testing the server paths in rootDSE"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                              attrs=["forestFunctionality", "domainFunctionality", "domainControllerFunctionality"])
        self.assertEquals(len(res), 1)
        self.assertEquals(len(res[0]["forestFunctionality"]), 1)
        self.assertEquals(len(res[0]["domainFunctionality"]), 1)
        self.assertEquals(len(res[0]["domainControllerFunctionality"]), 1)

        self.assertTrue(int(res[0]["forestFunctionality"][0]) <= int(res[0]["domainFunctionality"][0]))
        self.assertTrue(int(res[0]["domainControllerFunctionality"][0]) >= int(res[0]["domainFunctionality"][0]))

        res2 = self.ldb.search("", scope=SCOPE_BASE,
                              attrs=["dsServiceName", "serverName"])
        self.assertEquals(len(res2), 1)
        self.assertEquals(len(res2[0]["dsServiceName"]), 1)

        res3 = self.ldb.search(res2[0]["dsServiceName"][0], scope=SCOPE_BASE, attrs=["msDS-Behavior-Version"])
        self.assertEquals(len(res3), 1)
        self.assertEquals(len(res3[0]["msDS-Behavior-Version"]), 1)
        self.assertEquals(int(res[0]["domainControllerFunctionality"][0]), int(res3[0]["msDS-Behavior-Version"][0]))

        res4 = self.ldb.search(ldb.domain_dn(), scope=SCOPE_BASE, attrs=["msDS-Behavior-Version"])
        self.assertEquals(len(res4), 1)
        self.assertEquals(len(res4[0]["msDS-Behavior-Version"]), 1)
        self.assertEquals(int(res[0]["domainFunctionality"][0]), int(res4[0]["msDS-Behavior-Version"][0]))

        res5 = self.ldb.search("cn=partitions," + str(ldb.get_config_basedn()), scope=SCOPE_BASE, attrs=["msDS-Behavior-Version"])
        self.assertEquals(len(res5), 1)
        self.assertEquals(len(res5[0]["msDS-Behavior-Version"]), 1)
        self.assertEquals(int(res[0]["forestFunctionality"][0]), int(res5[0]["msDS-Behavior-Version"][0]))

    def test_dnsHostname(self):
        """Testing the DNS hostname in rootDSE"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                              attrs=["dnsHostName", "serverName"])
        self.assertEquals(len(res), 1)

        res2 = self.ldb.search(res[0]["serverName"][0], scope=SCOPE_BASE,
                               attrs=["dNSHostName"])
        self.assertEquals(len(res2), 1)

        self.assertEquals(res[0]["dnsHostName"][0], res2[0]["dNSHostName"][0])

    def test_ldapServiceName(self):
        """Testing the ldap service name in rootDSE"""
        res = self.ldb.search("", scope=SCOPE_BASE,
                              attrs=["ldapServiceName", "dNSHostName"])
        self.assertEquals(len(res), 1)

        (hostname, _, dns_domainname) = res[0]["dNSHostName"][0].partition(".")
        self.assertTrue(":%s$@%s" % (hostname, dns_domainname.upper())
                        in res[0]["ldapServiceName"][0])

if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host

ldb = SamDB(host, credentials=creds, session_info=system_session(lp), lp=lp)
if not "tdb://" in host:
    gc_ldb = Ldb("%s:3268" % host, credentials=creds,
                 session_info=system_session(lp), lp=lp)
else:
    gc_ldb = None

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(BaseDnTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(BasicTests)).wasSuccessful():
    rc = 1
sys.exit(rc)
