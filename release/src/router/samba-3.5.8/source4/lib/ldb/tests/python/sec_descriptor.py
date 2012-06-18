#!/usr/bin/python
# -*- coding: utf-8 -*-

import getopt
import optparse
import sys
import os
import base64
import re
import random

sys.path.append("bin/python")
sys.path.append("../lib/subunit/python")

import samba.getopt as options

# Some error messages that are being tested
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, LdbError
from ldb import ERR_NO_SUCH_OBJECT, ERR_INVALID_DN_SYNTAX, ERR_UNWILLING_TO_PERFORM
from ldb import ERR_INSUFFICIENT_ACCESS_RIGHTS

# For running the test unit
from samba.ndr import ndr_pack, ndr_unpack
from samba.dcerpc import security

from samba.auth import system_session
from samba import Ldb, DS_DOMAIN_FUNCTION_2008
from subunit import SubunitTestRunner
import unittest

parser = optparse.OptionParser("sec_descriptor [options] <host>")
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

#
# Tests start here
#

class DescriptorTests(unittest.TestCase):

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

    def find_domain_sid(self, ldb):
        res = ldb.search(base=self.base_dn, expression="(objectClass=*)", scope=SCOPE_BASE)
        return ndr_unpack( security.dom_sid,res[0]["objectSid"][0])

    def get_users_domain_dn(self, name):
        return "CN=%s,CN=Users,%s" % (name, self.base_dn)

    def modify_desc(self, object_dn, desc):
        assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
        mod = """
dn: """ + object_dn + """
changetype: modify
replace: nTSecurityDescriptor
"""
        if isinstance(desc, str):
            mod += "nTSecurityDescriptor: %s" % desc
        elif isinstance(desc, security.descriptor):
            mod += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        self.ldb_admin.modify_ldif(mod)

    def create_domain_ou(self, _ldb, ou_dn, desc=None):
        ldif = """
dn: """ + ou_dn + """
ou: """ + ou_dn.split(",")[0][3:] + """
objectClass: organizationalUnit
url: www.example.com
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def create_domain_user(self, _ldb, user_dn, desc=None):
        ldif = """
dn: """ + user_dn + """
sAMAccountName: """ + user_dn.split(",")[0][3:] + """
objectClass: user
userPassword: samba123@
url: www.example.com
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def create_domain_group(self, _ldb, group_dn, desc=None):
        ldif = """
dn: """ + group_dn + """
objectClass: group
sAMAccountName: """ + group_dn.split(",")[0][3:] + """
groupType: 4
url: www.example.com
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def get_unique_schema_class_name(self):
        while True:
            class_name = "test-class%s" % random.randint(1,100000)
            class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
            try:
                self.ldb_admin.search(base=class_dn, attrs=["*"])
            except LdbError, (num, _):
                self.assertEquals(num, ERR_NO_SUCH_OBJECT)
                return class_name

    def create_schema_class(self, _ldb, object_dn, desc=None):
        ldif = """
dn: """ + object_dn + """
objectClass: classSchema
objectCategory: CN=Class-Schema,""" + self.schema_dn + """
defaultObjectCategory: """ + object_dn + """
distinguishedName: """ + object_dn + """
governsID: 1.2.840.""" + str(random.randint(1,100000)) + """.1.5.9939
instanceType: 4
objectClassCategory: 1
subClassOf: organizationalPerson
systemFlags: 16
rDNAttID: cn
systemMustContain: cn
systemOnly: FALSE
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def create_configuration_container(self, _ldb, object_dn, desc=None):
        ldif = """
dn: """ + object_dn + """
objectClass: container
objectCategory: CN=Container,""" + self.schema_dn + """
showInAdvancedViewOnly: TRUE
instanceType: 4
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def create_configuration_specifier(self, _ldb, object_dn, desc=None):
        ldif = """
dn: """ + object_dn + """
objectClass: displaySpecifier
showInAdvancedViewOnly: TRUE
"""
        if desc:
            assert(isinstance(desc, str) or isinstance(desc, security.descriptor))
            if isinstance(desc, str):
                ldif += "nTSecurityDescriptor: %s" % desc
            elif isinstance(desc, security.descriptor):
                ldif += "nTSecurityDescriptor:: %s" % base64.b64encode(ndr_pack(desc))
        _ldb.add_ldif(ldif)

    def read_desc(self, object_dn):
        res = self.ldb_admin.search(base=object_dn, attrs=["nTSecurityDescriptor"])
        desc = res[0]["nTSecurityDescriptor"][0]
        return ndr_unpack( security.descriptor, desc )

    def enable_account(self,  user_dn):
        """Enable an account.
        :param user_dn: Dn of the account to enable.
        """
        res = self.ldb_admin.search(user_dn, SCOPE_BASE, None, ["userAccountControl"])
        assert len(res) == 1
        userAccountControl = res[0]["userAccountControl"][0]
        userAccountControl = int(userAccountControl)
        if (userAccountControl & 0x2):
            userAccountControl = userAccountControl & ~0x2 # remove disabled bit
        if (userAccountControl & 0x20):
            userAccountControl = userAccountControl & ~0x20 # remove 'no password required' bit
        mod = """
dn: """ + user_dn + """
changetype: modify
replace: userAccountControl
userAccountControl: %s""" % userAccountControl
        if self.WIN2003:
            mod = re.sub("userAccountControl: \d.*", "userAccountControl: 544", mod)
        self.ldb_admin.modify_ldif(mod)

    def get_ldb_connection(self, target_username, target_password):
        username_save = creds.get_username(); password_save = creds.get_password()
        creds.set_username(target_username)
        creds.set_password(target_password)
        ldb_target = Ldb(host, credentials=creds, session_info=system_session(), lp=lp)
        creds.set_username(username_save); creds.set_password(password_save)
        return ldb_target

    def get_object_sid(self, object_dn):
        res = self.ldb_admin.search(object_dn)
        return ndr_unpack( security.dom_sid, res[0]["objectSid"][0] )

    def dacl_add_ace(self, object_dn, ace):
        desc = self.read_desc( object_dn )
        desc_sddl = desc.as_sddl( self.domain_sid )
        if ace in desc_sddl:
            return
        if desc_sddl.find("(") >= 0:
            desc_sddl = desc_sddl[0:desc_sddl.index("(")] + ace + desc_sddl[desc_sddl.index("("):]
        else:
            desc_sddl = desc_sddl + ace
        self.modify_desc(object_dn, desc_sddl)

    def get_desc_sddl(self, object_dn):
        """ Return object nTSecutiryDescriptor in SDDL format
        """
        desc = self.read_desc(object_dn)
        return desc.as_sddl(self.domain_sid)

    def setUp(self):
        self.ldb_admin = ldb
        self.base_dn = self.find_basedn(self.ldb_admin)
        self.configuration_dn = self.find_configurationdn(self.ldb_admin)
        self.schema_dn = self.find_schemadn(self.ldb_admin)
        self.domain_sid = self.find_domain_sid(self.ldb_admin)
        print "baseDN: %s" % self.base_dn
        self.SAMBA = False; self.WIN2003 = False
        res = self.ldb_admin.search(base="", expression="", scope=SCOPE_BASE, attrs=["vendorName"])
        if "vendorName" in res[0].keys() and "Samba Team" in res[0]["vendorName"][0]:
            self.SAMBA = True
        else:
            self.WIN2003 = True
        #print "self.SAMBA:", self.SAMBA
        #print "self.WIN2003:", self.WIN2003

    ################################################################################################

    ## Tests for DOMAIN

    # Default descriptor tests #####################################################################

class OwnerGroupDescriptorTests(DescriptorTests):

    def setUp(self):
        DescriptorTests.setUp(self)
        if self.SAMBA:
            ### Create users
            # User 1
            user_dn = self.get_users_domain_dn("testuser1")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Enterprise Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 2
            user_dn = self.get_users_domain_dn("testuser2")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Domain Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 3
            user_dn = self.get_users_domain_dn("testuser3")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Schema Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 4
            user_dn = self.get_users_domain_dn("testuser4")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            # User 5
            user_dn = self.get_users_domain_dn("testuser5")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Enterprise Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn + """

dn: CN=Domain Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 6
            user_dn = self.get_users_domain_dn("testuser6")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Enterprise Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn + """

dn: CN=Domain Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn + """

dn: CN=Schema Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 7
            user_dn = self.get_users_domain_dn("testuser7")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Domain Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn + """

dn: CN=Schema Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
            # User 8
            user_dn = self.get_users_domain_dn("testuser8")
            self.create_domain_user(self.ldb_admin, user_dn)
            self.enable_account(user_dn)
            ldif = """
dn: CN=Enterprise Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn + """

dn: CN=Schema Admins,CN=Users,""" + self.base_dn + """
changetype: add
member: """ + user_dn
            self.ldb_admin.modify_ldif(ldif)
        self.results = {
            # msDS-Behavior-Version < DS_DOMAIN_FUNCTION_2008
            "ds_behavior_win2003" : {
                "100" : "O:EAG:DU",
                "101" : "O:DAG:DU",
                "102" : "O:%sG:DU",
                "103" : "O:%sG:DU",
                "104" : "O:DAG:DU",
                "105" : "O:DAG:DU",
                "106" : "O:DAG:DU",
                "107" : "O:EAG:DU",
                "108" : "O:DAG:DA",
                "109" : "O:DAG:DA",
                "110" : "O:%sG:DA",
                "111" : "O:%sG:DA",
                "112" : "O:DAG:DA",
                "113" : "O:DAG:DA",
                "114" : "O:DAG:DA",
                "115" : "O:DAG:DA",
                "130" : "O:EAG:DU",
                "131" : "O:DAG:DU",
                "132" : "O:SAG:DU",
                "133" : "O:%sG:DU",
                "134" : "O:EAG:DU",
                "135" : "O:SAG:DU",
                "136" : "O:SAG:DU",
                "137" : "O:SAG:DU",
                "138" : "O:DAG:DA",
                "139" : "O:DAG:DA",
                "140" : "O:%sG:DA",
                "141" : "O:%sG:DA",
                "142" : "O:DAG:DA",
                "143" : "O:DAG:DA",
                "144" : "O:DAG:DA",
                "145" : "O:DAG:DA",
                "160" : "O:EAG:DU",
                "161" : "O:DAG:DU",
                "162" : "O:%sG:DU",
                "163" : "O:%sG:DU",
                "164" : "O:EAG:DU",
                "165" : "O:EAG:DU",
                "166" : "O:DAG:DU",
                "167" : "O:EAG:DU",
                "168" : "O:DAG:DA",
                "169" : "O:DAG:DA",
                "170" : "O:%sG:DA",
                "171" : "O:%sG:DA",
                "172" : "O:DAG:DA",
                "173" : "O:DAG:DA",
                "174" : "O:DAG:DA",
                "175" : "O:DAG:DA",
            },
            # msDS-Behavior-Version >= 3
            "ds_behavior_win2008" : {
                "100" : "O:EAG:EA",
                "101" : "O:DAG:DA",
                "102" : "O:%sG:DU",
                "103" : "O:%sG:DU",
                "104" : "O:DAG:DA",
                "105" : "O:DAG:DA",
                "106" : "O:DAG:DA",
                "107" : "O:EAG:EA",
                "108" : "O:DAG:DA",
                "109" : "O:DAG:DA",
                "110" : "O:%sG:DA",
                "111" : "O:%sG:DA",
                "112" : "O:DAG:DA",
                "113" : "O:DAG:DA",
                "114" : "O:DAG:DA",
                "115" : "O:DAG:DA",
                "130" : "",
                "131" : "",
                "132" : "",
                "133" : "%s",
                "134" : "",
                "135" : "",
                "136" : "",
                "137" : "",
                "138" : "",
                "139" : "",
                "140" : "%s",
                "141" : "%s",
                "142" : "",
                "143" : "",
                "144" : "",
                "145" : "",
                "160" : "O:EAG:EA",
                "161" : "O:DAG:DA",
                "162" : "O:%sG:DU",
                "163" : "O:%sG:DU",
                "164" : "O:EAG:EA",
                "165" : "O:EAG:EA",
                "166" : "O:DAG:DA",
                "167" : "O:EAG:EA",
                "168" : "O:DAG:DA",
                "169" : "O:DAG:DA",
                "170" : "O:%sG:DA",
                "171" : "O:%sG:DA",
                "172" : "O:DAG:DA",
                "173" : "O:DAG:DA",
                "174" : "O:DAG:DA",
                "175" : "O:DAG:DA",
            },
        }
        # Discover 'msDS-Behavior-Version'
        res = self.ldb_admin.search(base=self.base_dn, expression="distinguishedName=%s" % self.base_dn, \
                attrs=['msDS-Behavior-Version'])
        res = int(res[0]['msDS-Behavior-Version'][0])
        if res < DS_DOMAIN_FUNCTION_2008:
            self.DS_BEHAVIOR = "ds_behavior_win2003"
        else:
            self.DS_BEHAVIOR = "ds_behavior_win2008"

    def tearDown(self):
        if self.SAMBA:
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser1"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser2"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser3"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser4"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser5"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser6"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser7"))
            self.delete_force(self.ldb_admin, self.get_users_domain_dn("testuser8"))
        # DOMAIN
        self.delete_force(self.ldb_admin, self.get_users_domain_dn("test_domain_group1"))
        self.delete_force(self.ldb_admin, "CN=test_domain_user1,OU=test_domain_ou1," + self.base_dn)
        self.delete_force(self.ldb_admin, "OU=test_domain_ou2,OU=test_domain_ou1," + self.base_dn)
        self.delete_force(self.ldb_admin, "OU=test_domain_ou1," + self.base_dn)
        # SCHEMA
        # CONFIGURATION
        self.delete_force(self.ldb_admin, "CN=test-specifier1,CN=test-container1,CN=DisplaySpecifiers," \
                + self.configuration_dn)
        self.delete_force(self.ldb_admin, "CN=test-container1,CN=DisplaySpecifiers," + self.configuration_dn)

    def check_user_belongs(self, user_dn, groups=[]):
        """ Test wether user is member of the expected group(s) """
        if groups != []:
            # User is member of at least one additional group
            res = self.ldb_admin.search(user_dn, attrs=["memberOf"])
            res = [x.upper() for x in sorted(list(res[0]["memberOf"]))]
            expected = []
            for x in groups:
                expected.append(self.get_users_domain_dn(x))
            expected = [x.upper() for x in sorted(expected)]
            self.assertEqual(expected, res)
        else:
            # User is not a member of any additional groups but default
            res = self.ldb_admin.search(user_dn, attrs=["*"])
            res = [x.upper() for x in res[0].keys()]
            self.assertFalse( "MEMBEROF" in res)

    def test_100(self):
        """ Enterprise admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection("testuser1", "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["100"], res)

    def test_101(self):
        """ Dmain admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["101"], res)

    def test_102(self):
        """ Schema admin group member with CC right creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        object_dn = "OU=test_domain_ou1," + self.base_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;%s)" % str(user_sid)
        self.dacl_add_ace(object_dn, mod)
        # Create additional object into the first one
        object_dn = "CN=test_domain_user1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_user(_ldb, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["102"] % str(user_sid), res)

    def test_103(self):
        """ Regular user with CC right creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        object_dn = "OU=test_domain_ou1," + self.base_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;%s)" % str(user_sid)
        self.dacl_add_ace(object_dn, mod)
        # Create additional object into the first one
        object_dn = "CN=test_domain_user1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_user(_ldb, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["103"] % str(user_sid), res)

    def test_104(self):
        """ Enterprise & Domain admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["104"], res)

    def test_105(self):
        """ Enterprise & Domain & Schema admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["105"], res)

    def test_106(self):
        """ Domain & Schema admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["106"], res)

    def test_107(self):
        """ Enterprise & Schema admin group member creates object (default nTSecurityDescriptor) in DOMAIN
        """
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        self.create_domain_group(_ldb, group_dn)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["107"], res)

    # Control descriptor tests #####################################################################

    def test_108(self):
        """ Enterprise admin group member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["108"], res)

    def test_109(self):
        """ Domain admin group member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["109"], res)

    def test_110(self):
        """ Schema admin group member with CC right creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        object_dn = "OU=test_domain_ou1," + self.base_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;%s)" % str(user_sid)
        self.dacl_add_ace(object_dn, mod)
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        # Create additional object into the first one
        object_dn = "CN=test_domain_user1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_user(_ldb, object_dn, desc_sddl)
        desc = self.read_desc(object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["110"] % str(user_sid), res)

    def test_111(self):
        """ Regular user with CC right creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        object_dn = "OU=test_domain_ou1," + self.base_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;%s)" % str(user_sid)
        self.dacl_add_ace(object_dn, mod)
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        # Create additional object into the first one
        object_dn = "CN=test_domain_user1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_user(_ldb, object_dn, desc_sddl)
        desc = self.read_desc(object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["111"] % str(user_sid), res)

    def test_112(self):
        """ Domain & Enterprise admin group member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["112"], res)

    def test_113(self):
        """ Domain & Enterprise & Schema admin group  member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["113"], res)

    def test_114(self):
        """ Domain & Schema admin group  member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["114"], res)

    def test_115(self):
        """ Enterprise & Schema admin group  member creates object (custom descriptor) in DOMAIN
        """
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        group_dn = "CN=test_domain_group1,CN=Users," + self.base_dn
        self.delete_force(self.ldb_admin, group_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_domain_group(_ldb, group_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(group_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["115"], res)


    def test_999(self):
        user_name = "Administrator"
        object_dn = "OU=test_domain_ou1," + self.base_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(D;CI;WP;;;S-1-3-0)"
        #mod = ""
        self.dacl_add_ace(object_dn, mod)
        desc_sddl = self.get_desc_sddl(object_dn)
        # Create additional object into the first one
        object_dn = "OU=test_domain_ou2," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_domain_ou(self.ldb_admin, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)

    ## Tests for SCHEMA

    # Defalt descriptor tests ##################################################################

    def test_130(self):
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["130"], res)

    def test_131(self):
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["131"], res)

    def test_132(self):
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["132"], res)

    def test_133(self):
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        #Change Schema partition descriptor
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["133"] % str(user_sid), res)

    def test_134(self):
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        #Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["134"], res)

    def test_135(self):
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["135"], res)

    def test_136(self):
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["136"], res)

    def test_137(self):
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["137"], res)

    # Custom descriptor tests ##################################################################

    def test_138(self):
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_139(self):
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_140(self):
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["140"] % str(user_sid), res)

    def test_141(self):
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["141"] % str(user_sid), res)

    def test_142(self):
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_143(self):
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_144(self):
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_145(self):
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Change Schema partition descriptor
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(self.schema_dn, mod)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        # Create example Schema class
        class_name = self.get_unique_schema_class_name()
        class_dn = "CN=%s,%s" % (class_name, self.schema_dn)
        self.create_schema_class(_ldb, class_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(class_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    ## Tests for CONFIGURATION

    # Defalt descriptor tests ##################################################################

    def test_160(self):
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["160"], res)

    def test_161(self):
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["161"], res)

    def test_162(self):
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        object_dn = "CN=test-container1,CN=DisplaySpecifiers," + self.configuration_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(self.ldb_admin, object_dn, )
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(object_dn, mod)
        # Create child object with user's credentials
        object_dn = "CN=test-specifier1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_specifier(_ldb, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["162"] % str(user_sid), res)

    def test_163(self):
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        object_dn = "CN=test-container1,CN=DisplaySpecifiers," + self.configuration_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(self.ldb_admin, object_dn, )
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(object_dn, mod)
        # Create child object with user's credentials
        object_dn = "CN=test-specifier1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_specifier(_ldb, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["163"] % str(user_sid), res)

    def test_164(self):
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["164"], res)

    def test_165(self):
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["165"], res)

    def test_166(self):
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["166"], res)

    def test_167(self):
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(_ldb, object_dn, )
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["167"], res)

    # Custom descriptor tests ##################################################################

    def test_168(self):
        user_name = "testuser1"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_169(self):
        user_name = "testuser2"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_170(self):
        user_name = "testuser3"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        object_dn = "CN=test-container1,CN=DisplaySpecifiers," + self.configuration_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(self.ldb_admin, object_dn, )
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(object_dn, mod)
        # Create child object with user's credentials
        object_dn = "CN=test-specifier1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        self.create_configuration_specifier(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["170"] % str(user_sid), res)

    def test_171(self):
        user_name = "testuser4"
        self.check_user_belongs(self.get_users_domain_dn(user_name), [])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        object_dn = "CN=test-container1,CN=DisplaySpecifiers," + self.configuration_dn
        self.delete_force(self.ldb_admin, object_dn)
        self.create_configuration_container(self.ldb_admin, object_dn, )
        user_sid = self.get_object_sid( self.get_users_domain_dn(user_name) )
        mod = "(A;;CC;;;AU)"
        self.dacl_add_ace(object_dn, mod)
        # Create child object with user's credentials
        object_dn = "CN=test-specifier1," + object_dn
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        # NB! Problematic owner part won't accept DA only <User Sid> !!!
        desc_sddl = "O:%sG:DAD:(A;;RP;;;DU)" % str(user_sid)
        self.create_configuration_specifier(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual(self.results[self.DS_BEHAVIOR]["171"] % str(user_sid), res)

    def test_172(self):
        user_name = "testuser5"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_173(self):
        user_name = "testuser6"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_174(self):
        user_name = "testuser7"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Domain Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    def test_175(self):
        user_name = "testuser8"
        self.check_user_belongs(self.get_users_domain_dn(user_name), ["Enterprise Admins", "Schema Admins"])
        # Open Ldb connection with the tested user
        _ldb = self.get_ldb_connection(user_name, "samba123@")
        # Create example Configuration container
        container_name = "test-container1"
        object_dn = "CN=%s,CN=DisplaySpecifiers,%s" % (container_name, self.configuration_dn)
        self.delete_force(self.ldb_admin, object_dn)
        # Create a custom security descriptor
        desc_sddl = "O:DAG:DAD:(A;;RP;;;DU)"
        self.create_configuration_container(_ldb, object_dn, desc_sddl)
        desc_sddl = self.get_desc_sddl(object_dn)
        res = re.search("(O:.*G:.*?)D:", desc_sddl).group(1)
        self.assertEqual("O:DAG:DA", res)

    ########################################################################################
    # Inharitance tests for DACL

class DaclDescriptorTests(DescriptorTests):

    def setUp(self):
        DescriptorTests.setUp(self)

    def tearDown(self):
        self.delete_force(self.ldb_admin, "CN=test_inherit_group,OU=test_inherit_ou," + self.base_dn)
        self.delete_force(self.ldb_admin, "OU=test_inherit_ou," + self.base_dn)

    def create_clean_ou(self, object_dn):
        """ Base repeating setup for unittests to follow """
        res = self.ldb_admin.search(base=self.base_dn, scope=SCOPE_SUBTREE, \
                expression="distinguishedName=%s" % object_dn)
        # Make sure top testing OU has been deleted before starting the test
        self.assertEqual(res, [])
        self.create_domain_ou(self.ldb_admin, object_dn)
        desc_sddl = self.get_desc_sddl(object_dn)
        # Make sutre there are inheritable ACEs initially
        self.assertTrue("CI" in desc_sddl or "OI" in desc_sddl)
        # Find and remove all inherit ACEs
        res = re.findall("\(.*?\)", desc_sddl)
        res = [x for x in res if ("CI" in x) or ("OI" in x)]
        for x in res:
            desc_sddl = desc_sddl.replace(x, "")
        # Add flag 'protected' in both DACL and SACL so no inherit ACEs
        # can propagate from above
        desc_sddl = desc_sddl.replace(":AI", ":AIP")
        # colon at the end breaks ldif parsing, fix it
        res = re.findall(".*?S:", desc_sddl)
        if res:
            desc_sddl = desc_sddl.replace("S:", "")
        self.modify_desc(object_dn, desc_sddl)
        # Verify all inheritable ACEs are gone
        desc_sddl = self.get_desc_sddl(object_dn)
        self.assertFalse("CI" in desc_sddl)
        self.assertFalse("OI" in desc_sddl)

    def test_200(self):
        """ OU with protected flag and child group. See if the group has inherit ACEs.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn)
        # Make sure created group object contains NO inherit ACEs
        desc_sddl = self.get_desc_sddl(group_dn)
        self.assertFalse("ID" in desc_sddl)

    def test_201(self):
        """ OU with protected flag and no inherit ACEs, child group with custom descriptor.
            Verify group has custom and default ACEs only.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Create group child object using custom security descriptor
        sddl = "O:AUG:AUD:AI(D;;WP;;;DU)"
        self.create_domain_group(self.ldb_admin, group_dn, sddl)
        # Make sure created group descriptor has NO additional ACEs
        desc_sddl = self.get_desc_sddl(group_dn)
        print "group descriptor: " + desc_sddl
        self.assertEqual(desc_sddl, sddl)

    def test_202(self):
        """ OU with protected flag and add couple non-inheritable ACEs, child group.
            See if the group has any of the added ACEs.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom non-inheritable ACEs
        mod = "(D;;WP;;;DU)(A;;RP;;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        # Verify all inheritable ACEs are gone
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn)
        # Make sure created group object contains NO inherit ACEs
        # also make sure the added above non-inheritable ACEs are absant too
        desc_sddl = self.get_desc_sddl(group_dn)
        self.assertFalse("ID" in desc_sddl)
        for x in re.findall("\(.*?\)", mod):
            self.assertFalse(x in desc_sddl)

    def test_203(self):
        """ OU with protected flag and add 'CI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'CI' ACE
        mod = "(D;CI;WP;;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";CI;", ";CIID;")
        self.assertTrue(mod in desc_sddl)

    def test_204(self):
        """ OU with protected flag and add 'OI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'CI' ACE
        mod = "(D;OI;WP;;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";OI;", ";OIIOID;") # change it how it's gonna look like
        self.assertTrue(mod in desc_sddl)

    def test_205(self):
        """ OU with protected flag and add 'OA' for GUID & 'CI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'OA' for 'name' attribute & 'CI' ACE
        mod = "(OA;CI;WP;bf967a0e-0de6-11d0-a285-00aa003049e2;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";CI;", ";CIID;") # change it how it's gonna look like
        self.assertTrue(mod in desc_sddl)

    def test_206(self):
        """ OU with protected flag and add 'OA' for GUID & 'OI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'OA' for 'name' attribute & 'OI' ACE
        mod = "(OA;OI;WP;bf967a0e-0de6-11d0-a285-00aa003049e2;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";OI;", ";OIIOID;") # change it how it's gonna look like
        self.assertTrue(mod in desc_sddl)

    def test_207(self):
        """ OU with protected flag and add 'OA' for OU specific GUID & 'CI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'OA' for 'st' attribute (OU specific) & 'CI' ACE
        mod = "(OA;CI;WP;bf967a39-0de6-11d0-a285-00aa003049e2;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";CI;", ";CIID;") # change it how it's gonna look like
        self.assertTrue(mod in desc_sddl)

    def test_208(self):
        """ OU with protected flag and add 'OA' for OU specific GUID & 'OI' ACE, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'OA' for 'st' attribute (OU specific) & 'OI' ACE
        mod = "(OA;OI;WP;bf967a39-0de6-11d0-a285-00aa003049e2;;DU)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        mod = mod.replace(";OI;", ";OIIOID;") # change it how it's gonna look like
        self.assertTrue(mod in desc_sddl)

    def test_209(self):
        """ OU with protected flag and add 'CI' ACE with 'CO' SID, child group.
            See if the group has the added inherited ACE.
        """
        ou_dn = "OU=test_inherit_ou," + self.base_dn
        group_dn = "CN=test_inherit_group," + ou_dn
        # Create inheritable-free OU
        self.create_clean_ou(ou_dn)
        # Add some custom 'CI' ACE
        mod = "(D;CI;WP;;;CO)"
        self.dacl_add_ace(ou_dn, mod)
        desc_sddl = self.get_desc_sddl(ou_dn)
        # Create group child object
        self.create_domain_group(self.ldb_admin, group_dn, "O:AUG:AUD:AI(A;;CC;;;AU)")
        # Make sure created group object contains only the above inherited ACE(s)
        # that we've added manually
        desc_sddl = self.get_desc_sddl(group_dn)
        self.assertTrue("(D;ID;WP;;;AU)" in desc_sddl)
        self.assertTrue("(D;CIIOID;WP;;;CO)" in desc_sddl)

    ########################################################################################

if not "://" in host:
    host = "ldap://%s" % host
ldb = Ldb(host, credentials=creds, session_info=system_session(), lp=lp, options=["modules:paged_searches"])

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(OwnerGroupDescriptorTests)).wasSuccessful():
    rc = 1
if not runner.run(unittest.makeSuite(DaclDescriptorTests)).wasSuccessful():
    rc = 1

sys.exit(rc)
