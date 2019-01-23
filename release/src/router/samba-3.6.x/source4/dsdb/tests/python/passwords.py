#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This tests the password changes over LDAP for AD implementations
#
# Copyright Matthias Dieter Wallnoefer 2010
#
# Notice: This tests will also work against Windows Server if the connection is
# secured enough (SASL with a minimum of 128 Bit encryption) - consider
# MS-ADTS 3.1.1.3.1.5

import optparse
import sys
import base64
import time
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from samba.credentials import Credentials
from ldb import SCOPE_BASE, LdbError
from ldb import ERR_ATTRIBUTE_OR_VALUE_EXISTS
from ldb import ERR_UNWILLING_TO_PERFORM, ERR_INSUFFICIENT_ACCESS_RIGHTS
from ldb import ERR_NO_SUCH_ATTRIBUTE
from ldb import ERR_CONSTRAINT_VIOLATION
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_ADD, FLAG_MOD_REPLACE, FLAG_MOD_DELETE
from samba import gensec
from samba.samdb import SamDB
import samba.tests
from samba.tests import delete_force
from subunit.run import SubunitTestRunner
import unittest

parser = optparse.OptionParser("passwords.py [options] <host>")
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

# Force an encrypted connection
creds.set_gensec_features(creds.get_gensec_features() | gensec.FEATURE_SEAL)

#
# Tests start here
#

class PasswordTests(samba.tests.TestCase):

    def setUp(self):
        super(PasswordTests, self).setUp()
        self.ldb = ldb
        self.base_dn = ldb.domain_dn()

        # (Re)adds the test user "testuser" with no password atm
        delete_force(self.ldb, "cn=testuser,cn=users," + self.base_dn)
        self.ldb.add({
             "dn": "cn=testuser,cn=users," + self.base_dn,
             "objectclass": "user",
             "sAMAccountName": "testuser"})

        # Tests a password change when we don't have any password yet with a
        # wrong old password
        try:
            self.ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: noPassword
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
            # Windows (2008 at least) seems to have some small bug here: it
            # returns "0000056A" on longer (always wrong) previous passwords.
            self.assertTrue('00000056' in msg)

        # Sets the initial user password with a "special" password change
        # I think that this internally is a password set operation and it can
        # only be performed by someone which has password set privileges on the
        # account (at least in s4 we do handle it like that).
        self.ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
add: userPassword
userPassword: thatsAcomplPASS1
""")

        # But in the other way around this special syntax doesn't work
        try:
            self.ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        # Enables the user account
        self.ldb.enable_account("(sAMAccountName=testuser)")

        # Open a second LDB connection with the user credentials. Use the
        # command line credentials for informations like the domain, the realm
        # and the workstation.
        creds2 = Credentials()
        creds2.set_username("testuser")
        creds2.set_password("thatsAcomplPASS1")
        creds2.set_domain(creds.get_domain())
        creds2.set_realm(creds.get_realm())
        creds2.set_workstation(creds.get_workstation())
        creds2.set_gensec_features(creds2.get_gensec_features()
                                                          | gensec.FEATURE_SEAL)
        self.ldb2 = SamDB(url=host, credentials=creds2, lp=lp)

    def test_unicodePwd_hash_set(self):
        print "Performs a password hash set operation on 'unicodePwd' which should be prevented"
        # Notice: Direct hash password sets should never work

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["unicodePwd"] = MessageElement("XXXXXXXXXXXXXXXX", FLAG_MOD_REPLACE,
          "unicodePwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

    def test_unicodePwd_hash_change(self):
        print "Performs a password hash change operation on 'unicodePwd' which should be prevented"
        # Notice: Direct hash password changes should never work

        # Hash password changes should never work
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: unicodePwd
unicodePwd: XXXXXXXXXXXXXXXX
add: unicodePwd
unicodePwd: YYYYYYYYYYYYYYYY
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

    def test_unicodePwd_clear_set(self):
        print "Performs a password cleartext set operation on 'unicodePwd'"

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["unicodePwd"] = MessageElement("\"thatsAcomplPASS2\"".encode('utf-16-le'),
          FLAG_MOD_REPLACE, "unicodePwd")
        ldb.modify(m)

    def test_unicodePwd_clear_change(self):
        print "Performs a password cleartext change operation on 'unicodePwd'"

        self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS1\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")

        # Wrong old password
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS3\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS4\"".encode('utf-16-le')) + """
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
            self.assertTrue('00000056' in msg)

        # A change to the same password again will not work (password history)
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS2\"".encode('utf-16-le')) + """
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
            self.assertTrue('0000052D' in msg)

    def test_dBCSPwd_hash_set(self):
        print "Performs a password hash set operation on 'dBCSPwd' which should be prevented"
        # Notice: Direct hash password sets should never work

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["dBCSPwd"] = MessageElement("XXXXXXXXXXXXXXXX", FLAG_MOD_REPLACE,
          "dBCSPwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

    def test_dBCSPwd_hash_change(self):
        print "Performs a password hash change operation on 'dBCSPwd' which should be prevented"
        # Notice: Direct hash password changes should never work

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: dBCSPwd
dBCSPwd: XXXXXXXXXXXXXXXX
add: dBCSPwd
dBCSPwd: YYYYYYYYYYYYYYYY
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

    def test_userPassword_clear_set(self):
        print "Performs a password cleartext set operation on 'userPassword'"
        # Notice: This works only against Windows if "dSHeuristics" has been set
        # properly

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement("thatsAcomplPASS2", FLAG_MOD_REPLACE,
          "userPassword")
        ldb.modify(m)

    def test_userPassword_clear_change(self):
        print "Performs a password cleartext change operation on 'userPassword'"
        # Notice: This works only against Windows if "dSHeuristics" has been set
        # properly

        self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")

        # Wrong old password
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS3
add: userPassword
userPassword: thatsAcomplPASS4
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
            self.assertTrue('00000056' in msg)

        # A change to the same password again will not work (password history)
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS2
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, msg):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
            self.assertTrue('0000052D' in msg)

    def test_clearTextPassword_clear_set(self):
        print "Performs a password cleartext set operation on 'clearTextPassword'"
        # Notice: This never works against Windows - only supported by us

        try:
            m = Message()
            m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
            m["clearTextPassword"] = MessageElement("thatsAcomplPASS2".encode('utf-16-le'),
              FLAG_MOD_REPLACE, "clearTextPassword")
            ldb.modify(m)
            # this passes against s4
        except LdbError, (num, msg):
            # "NO_SUCH_ATTRIBUTE" is returned by Windows -> ignore it
            if num != ERR_NO_SUCH_ATTRIBUTE:
                raise LdbError(num, msg)

    def test_clearTextPassword_clear_change(self):
        print "Performs a password cleartext change operation on 'clearTextPassword'"
        # Notice: This never works against Windows - only supported by us

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS1".encode('utf-16-le')) + """
add: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS2".encode('utf-16-le')) + """
""")
            # this passes against s4
        except LdbError, (num, msg):
            # "NO_SUCH_ATTRIBUTE" is returned by Windows -> ignore it
            if num != ERR_NO_SUCH_ATTRIBUTE:
                raise LdbError(num, msg)

        # Wrong old password
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS3".encode('utf-16-le')) + """
add: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS4".encode('utf-16-le')) + """
""")
            self.fail()
        except LdbError, (num, msg):
            # "NO_SUCH_ATTRIBUTE" is returned by Windows -> ignore it
            if num != ERR_NO_SUCH_ATTRIBUTE:
                self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
                self.assertTrue('00000056' in msg)

        # A change to the same password again will not work (password history)
        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS2".encode('utf-16-le')) + """
add: clearTextPassword
clearTextPassword:: """ + base64.b64encode("thatsAcomplPASS2".encode('utf-16-le')) + """
""")
            self.fail()
        except LdbError, (num, msg):
            # "NO_SUCH_ATTRIBUTE" is returned by Windows -> ignore it
            if num != ERR_NO_SUCH_ATTRIBUTE:
                self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)
                self.assertTrue('0000052D' in msg)

    def test_failures(self):
        print "Performs some failure testing"

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
add: userPassword
userPassword: thatsAcomplPASS1
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
add: userPassword
userPassword: thatsAcomplPASS1
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        try:
            ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
replace: userPassword
userPassword: thatsAcomplPASS3
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS1
add: userPassword
userPassword: thatsAcomplPASS2
replace: userPassword
userPassword: thatsAcomplPASS3
""")
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_INSUFFICIENT_ACCESS_RIGHTS)

        # Reverse order does work
        self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
add: userPassword
userPassword: thatsAcomplPASS2
delete: userPassword
userPassword: thatsAcomplPASS1
""")

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: userPassword
userPassword: thatsAcomplPASS2
add: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS3\"".encode('utf-16-le')) + """
""")
             # this passes against s4
        except LdbError, (num, _):
            self.assertEquals(num, ERR_ATTRIBUTE_OR_VALUE_EXISTS)

        try:
            self.ldb2.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
delete: unicodePwd
unicodePwd:: """ + base64.b64encode("\"thatsAcomplPASS3\"".encode('utf-16-le')) + """
add: userPassword
userPassword: thatsAcomplPASS4
""")
             # this passes against s4
        except LdbError, (num, _):
            self.assertEquals(num, ERR_NO_SUCH_ATTRIBUTE)

        # Several password changes at once are allowed
        ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
userPassword: thatsAcomplPASS2
""")

        # Several password changes at once are allowed
        ldb.modify_ldif("""
dn: cn=testuser,cn=users,""" + self.base_dn + """
changetype: modify
replace: userPassword
userPassword: thatsAcomplPASS1
userPassword: thatsAcomplPASS2
replace: userPassword
userPassword: thatsAcomplPASS3
replace: userPassword
userPassword: thatsAcomplPASS4
""")

        # This surprisingly should work
        delete_force(self.ldb, "cn=testuser2,cn=users," + self.base_dn)
        self.ldb.add({
             "dn": "cn=testuser2,cn=users," + self.base_dn,
             "objectclass": "user",
             "userPassword": ["thatsAcomplPASS1", "thatsAcomplPASS2"] })

        # This surprisingly should work
        delete_force(self.ldb, "cn=testuser2,cn=users," + self.base_dn)
        self.ldb.add({
             "dn": "cn=testuser2,cn=users," + self.base_dn,
             "objectclass": "user",
             "userPassword": ["thatsAcomplPASS1", "thatsAcomplPASS1"] })

    def test_empty_passwords(self):
        print "Performs some empty passwords testing"

        try:
            self.ldb.add({
                 "dn": "cn=testuser2,cn=users," + self.base_dn,
                 "objectclass": "user",
                 "unicodePwd": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb.add({
                 "dn": "cn=testuser2,cn=users," + self.base_dn,
                 "objectclass": "user",
                 "dBCSPwd": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb.add({
                 "dn": "cn=testuser2,cn=users," + self.base_dn,
                 "objectclass": "user",
                 "userPassword": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        try:
            self.ldb.add({
                 "dn": "cn=testuser2,cn=users," + self.base_dn,
                 "objectclass": "user",
                 "clearTextPassword": [] })
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num == ERR_CONSTRAINT_VIOLATION or
                            num == ERR_NO_SUCH_ATTRIBUTE) # for Windows

        delete_force(self.ldb, "cn=testuser2,cn=users," + self.base_dn)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["unicodePwd"] = MessageElement([], FLAG_MOD_ADD, "unicodePwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["dBCSPwd"] = MessageElement([], FLAG_MOD_ADD, "dBCSPwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement([], FLAG_MOD_ADD, "userPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["clearTextPassword"] = MessageElement([], FLAG_MOD_ADD, "clearTextPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num == ERR_CONSTRAINT_VIOLATION or
                            num == ERR_NO_SUCH_ATTRIBUTE) # for Windows

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["unicodePwd"] = MessageElement([], FLAG_MOD_REPLACE, "unicodePwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["dBCSPwd"] = MessageElement([], FLAG_MOD_REPLACE, "dBCSPwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement([], FLAG_MOD_REPLACE, "userPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["clearTextPassword"] = MessageElement([], FLAG_MOD_REPLACE, "clearTextPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num == ERR_UNWILLING_TO_PERFORM or
                            num == ERR_NO_SUCH_ATTRIBUTE) # for Windows

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["unicodePwd"] = MessageElement([], FLAG_MOD_DELETE, "unicodePwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["dBCSPwd"] = MessageElement([], FLAG_MOD_DELETE, "dBCSPwd")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_UNWILLING_TO_PERFORM)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement([], FLAG_MOD_DELETE, "userPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertEquals(num, ERR_CONSTRAINT_VIOLATION)

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["clearTextPassword"] = MessageElement([], FLAG_MOD_DELETE, "clearTextPassword")
        try:
            ldb.modify(m)
            self.fail()
        except LdbError, (num, _):
            self.assertTrue(num == ERR_CONSTRAINT_VIOLATION or
                            num == ERR_NO_SUCH_ATTRIBUTE) # for Windows

    def test_plain_userPassword(self):
        print "Performs testing about the standard 'userPassword' behaviour"

        # Delete the "dSHeuristics"
        ldb.set_dsheuristics(None)

        time.sleep(1) # This switching time is strictly needed!

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement("myPassword", FLAG_MOD_ADD,
          "userPassword")
        ldb.modify(m)

        res = ldb.search("cn=testuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["userPassword"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("userPassword" in res[0])
        self.assertEquals(res[0]["userPassword"][0], "myPassword")

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement("myPassword2", FLAG_MOD_REPLACE,
          "userPassword")
        ldb.modify(m)

        res = ldb.search("cn=testuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["userPassword"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("userPassword" in res[0])
        self.assertEquals(res[0]["userPassword"][0], "myPassword2")

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement([], FLAG_MOD_DELETE,
          "userPassword")
        ldb.modify(m)

        res = ldb.search("cn=testuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["userPassword"])
        self.assertTrue(len(res) == 1)
        self.assertFalse("userPassword" in res[0])

        # Set the test "dSHeuristics" to deactivate "userPassword" pwd changes
        ldb.set_dsheuristics("000000000")

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement("myPassword3", FLAG_MOD_REPLACE,
          "userPassword")
        ldb.modify(m)

        res = ldb.search("cn=testuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["userPassword"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("userPassword" in res[0])
        self.assertEquals(res[0]["userPassword"][0], "myPassword3")

        # Set the test "dSHeuristics" to deactivate "userPassword" pwd changes
        ldb.set_dsheuristics("000000002")

        m = Message()
        m.dn = Dn(ldb, "cn=testuser,cn=users," + self.base_dn)
        m["userPassword"] = MessageElement("myPassword4", FLAG_MOD_REPLACE,
          "userPassword")
        ldb.modify(m)

        res = ldb.search("cn=testuser,cn=users," + self.base_dn,
                         scope=SCOPE_BASE, attrs=["userPassword"])
        self.assertTrue(len(res) == 1)
        self.assertTrue("userPassword" in res[0])
        self.assertEquals(res[0]["userPassword"][0], "myPassword4")

        # Reset the test "dSHeuristics" (reactivate "userPassword" pwd changes)
        ldb.set_dsheuristics("000000001")

    def test_zero_length(self):
        # Get the old "minPwdLength"
        minPwdLength = ldb.get_minPwdLength()
        # Set it temporarely to "0"
        ldb.set_minPwdLength("0")

        # Get the old "pwdProperties"
        pwdProperties = ldb.get_pwdProperties()
        # Set them temporarely to "0" (to deactivate eventually the complexity)
        ldb.set_pwdProperties("0")

        ldb.setpassword("(sAMAccountName=testuser)", "")

        # Reset the "pwdProperties" as they were before
        ldb.set_pwdProperties(pwdProperties)

        # Reset the "minPwdLength" as it was before
        ldb.set_minPwdLength(minPwdLength)

    def tearDown(self):
        super(PasswordTests, self).tearDown()
        delete_force(self.ldb, "cn=testuser,cn=users," + self.base_dn)
        delete_force(self.ldb, "cn=testuser2,cn=users," + self.base_dn)
        # Close the second LDB connection (with the user credentials)
        self.ldb2 = None

if not "://" in host:
    if os.path.isfile(host):
        host = "tdb://%s" % host
    else:
        host = "ldap://%s" % host

ldb = SamDB(url=host, session_info=system_session(lp), credentials=creds, lp=lp)

# Gets back the basedn
base_dn = ldb.domain_dn()

# Gets back the configuration basedn
configuration_dn = ldb.get_config_basedn().get_linearized()

# Get the old "dSHeuristics" if it was set
dsheuristics = ldb.get_dsheuristics()

# Set the "dSHeuristics" to activate the correct "userPassword" behaviour
ldb.set_dsheuristics("000000001")

# Get the old "minPwdAge"
minPwdAge = ldb.get_minPwdAge()

# Set it temporarely to "0"
ldb.set_minPwdAge("0")

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(PasswordTests)).wasSuccessful():
    rc = 1

# Reset the "dSHeuristics" as they were before
ldb.set_dsheuristics(dsheuristics)

# Reset the "minPwdAge" as it was before
ldb.set_minPwdAge(minPwdAge)

sys.exit(rc)
