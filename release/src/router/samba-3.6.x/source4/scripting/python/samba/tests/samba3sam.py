#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2005-2008
# Copyright (C) Martin Kuehl <mkhl@samba.org> 2006
#
# This is a Python port of the original in testprogs/ejs/samba3sam.js
#   
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#   
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#   
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""Tests for the samba3sam LDB module, which maps Samba3 LDAP to AD LDAP."""

import os
import ldb
from ldb import SCOPE_DEFAULT, SCOPE_BASE
from samba import Ldb, substitute_var
from samba.tests import TestCaseInTempDir, env_loadparm
import samba.dcerpc.security
import samba.ndr
from samba.auth import system_session


def read_datafile(filename):
    paths = [ "../../../../../testdata/samba3",
              "../../../../testdata/samba3" ]
    for p in paths:
        datadir = os.path.join(os.path.dirname(__file__), p)
        if os.path.exists(datadir):
            break
    return open(os.path.join(datadir, filename), 'r').read()

def ldb_debug(l, text):
    print text


class MapBaseTestCase(TestCaseInTempDir):
    """Base test case for mapping tests."""

    def setup_modules(self, ldb, s3, s4):
        ldb.add({"dn": "@MAP=samba3sam",
                 "@FROM": s4.basedn,
                 "@TO": "sambaDomainName=TESTS," + s3.basedn})

        ldb.add({"dn": "@MODULES",
                 "@LIST": "rootdse,paged_results,server_sort,asq,samldb,password_hash,operational,objectguid,rdn_name,samba3sam,samba3sid,partition"})

        ldb.add({"dn": "@PARTITION",
            "partition": ["%s" % (s4.basedn_casefold), 
                          "%s" % (s3.basedn_casefold)],
            "replicateEntries": ["@ATTRIBUTES", "@INDEXLIST"],
            "modules": "*:"})

    def setUp(self):
        self.lp = env_loadparm()
        self.lp.set("sid generator", "backend")
        self.lp.set("workgroup", "TESTS")
        self.lp.set("netbios name", "TESTS")
        super(MapBaseTestCase, self).setUp()

        def make_dn(basedn, rdn):
            return "%s,sambaDomainName=TESTS,%s" % (rdn, basedn)

        def make_s4dn(basedn, rdn):
            return "%s,%s" % (rdn, basedn)

        self.ldbfile = os.path.join(self.tempdir, "test.ldb")
        self.ldburl = "tdb://" + self.ldbfile

        tempdir = self.tempdir

        class Target:
            """Simple helper class that contains data for a specific SAM 
            connection."""

            def __init__(self, basedn, dn, lp):
                self.db = Ldb(lp=lp, session_info=system_session())
                self.basedn = basedn
                self.basedn_casefold = ldb.Dn(self.db, basedn).get_casefold()
                self.substvars = {"BASEDN": self.basedn}
                self.file = os.path.join(tempdir, "%s.ldb" % self.basedn_casefold)
                self.url = "tdb://" + self.file
                self._dn = dn

            def dn(self, rdn):
                return self._dn(self.basedn, rdn)

            def connect(self):
                return self.db.connect(self.url)

            def setup_data(self, path):
                self.add_ldif(read_datafile(path))

            def subst(self, text):
                return substitute_var(text, self.substvars)

            def add_ldif(self, ldif):
                self.db.add_ldif(self.subst(ldif))

            def modify_ldif(self, ldif):
                self.db.modify_ldif(self.subst(ldif))

        self.samba4 = Target("dc=vernstok,dc=nl", make_s4dn, self.lp)
        self.samba3 = Target("cn=Samba3Sam", make_dn, self.lp)

        self.samba3.connect()
        self.samba4.connect()

    def tearDown(self):
        os.unlink(self.ldbfile)
        os.unlink(self.samba3.file)
        os.unlink(self.samba4.file)
        super(MapBaseTestCase, self).tearDown()

    def assertSidEquals(self, text, ndr_sid):
        sid_obj1 = samba.ndr.ndr_unpack(samba.dcerpc.security.dom_sid,
                                        str(ndr_sid[0]))
        sid_obj2 = samba.dcerpc.security.dom_sid(text)
        self.assertEquals(sid_obj1, sid_obj2)


class Samba3SamTestCase(MapBaseTestCase):

    def setUp(self):
        super(Samba3SamTestCase, self).setUp()
        ldb = Ldb(self.ldburl, lp=self.lp, session_info=system_session())
        self.samba3.setup_data("samba3.ldif")
        ldif = read_datafile("provision_samba3sam.ldif")
        ldb.add_ldif(self.samba4.subst(ldif))
        self.setup_modules(ldb, self.samba3, self.samba4)
        del ldb
        self.ldb = Ldb(self.ldburl, lp=self.lp, session_info=system_session())

    def test_search_non_mapped(self):
        """Looking up by non-mapped attribute"""
        msg = self.ldb.search(expression="(cn=Administrator)")
        self.assertEquals(len(msg), 1)
        self.assertEquals(msg[0]["cn"], "Administrator")

    def test_search_non_mapped(self):
        """Looking up by mapped attribute"""
        msg = self.ldb.search(expression="(name=Backup Operators)")
        self.assertEquals(len(msg), 1)
        self.assertEquals(str(msg[0]["name"]), "Backup Operators")

    def test_old_name_of_renamed(self):
        """Looking up by old name of renamed attribute"""
        msg = self.ldb.search(expression="(displayName=Backup Operators)")
        self.assertEquals(len(msg), 0)

    def test_mapped_containing_sid(self):
        """Looking up mapped entry containing SID"""
        msg = self.ldb.search(expression="(cn=Replicator)")
        self.assertEquals(len(msg), 1)
        self.assertEquals(str(msg[0].dn), 
                          "cn=Replicator,ou=Groups,dc=vernstok,dc=nl")
        self.assertTrue("objectSid" in msg[0]) 
        self.assertSidEquals("S-1-5-21-4231626423-2410014848-2360679739-552",
                             msg[0]["objectSid"])
        oc = set(msg[0]["objectClass"])
        self.assertEquals(oc, set(["group"]))

    def test_search_by_objclass(self):
        """Looking up by objectClass"""
        msg = self.ldb.search(expression="(|(objectClass=user)(cn=Administrator))")
        self.assertEquals(set([str(m.dn) for m in msg]), 
                set(["unixName=Administrator,ou=Users,dc=vernstok,dc=nl", 
                     "unixName=nobody,ou=Users,dc=vernstok,dc=nl"]))

    def test_s3sam_modify(self):
        # Adding a record that will be fallbacked
        self.ldb.add({"dn": "cn=Foo", 
            "foo": "bar", 
            "blah": "Blie", 
            "cn": "Foo", 
            "showInAdvancedViewOnly": "TRUE"}
            )

        # Checking for existence of record (local)
        # TODO: This record must be searched in the local database, which is 
        # currently only supported for base searches
        # msg = ldb.search(expression="(cn=Foo)", ['foo','blah','cn','showInAdvancedViewOnly')]
        # TODO: Actually, this version should work as well but doesn't...
        # 
        #    
        msg = self.ldb.search(expression="(cn=Foo)", base="cn=Foo", 
                scope=SCOPE_BASE, 
                attrs=['foo','blah','cn','showInAdvancedViewOnly'])
        self.assertEquals(len(msg), 1)
        self.assertEquals(str(msg[0]["showInAdvancedViewOnly"]), "TRUE")
        self.assertEquals(str(msg[0]["foo"]), "bar")
        self.assertEquals(str(msg[0]["blah"]), "Blie")

        # Adding record that will be mapped
        self.ldb.add({"dn": "cn=Niemand,cn=Users,dc=vernstok,dc=nl",
                 "objectClass": "user",
                 "unixName": "bin",
                 "sambaUnicodePwd": "geheim",
                 "cn": "Niemand"})

        # Checking for existence of record (remote)
        msg = self.ldb.search(expression="(unixName=bin)", 
                              attrs=['unixName','cn','dn', 'sambaUnicodePwd'])
        self.assertEquals(len(msg), 1)
        self.assertEquals(str(msg[0]["cn"]), "Niemand")
        self.assertEquals(str(msg[0]["sambaUnicodePwd"]), "geheim")

        # Checking for existence of record (local && remote)
        msg = self.ldb.search(expression="(&(unixName=bin)(sambaUnicodePwd=geheim))", 
                         attrs=['unixName','cn','dn', 'sambaUnicodePwd'])
        self.assertEquals(len(msg), 1)           # TODO: should check with more records
        self.assertEquals(str(msg[0]["cn"]), "Niemand")
        self.assertEquals(str(msg[0]["unixName"]), "bin")
        self.assertEquals(str(msg[0]["sambaUnicodePwd"]), "geheim")

        # Checking for existence of record (local || remote)
        msg = self.ldb.search(expression="(|(unixName=bin)(sambaUnicodePwd=geheim))", 
                         attrs=['unixName','cn','dn', 'sambaUnicodePwd'])
        #print "got %d replies" % len(msg)
        self.assertEquals(len(msg), 1)        # TODO: should check with more records
        self.assertEquals(str(msg[0]["cn"]), "Niemand")
        self.assertEquals(str(msg[0]["unixName"]), "bin")
        self.assertEquals(str(msg[0]["sambaUnicodePwd"]), "geheim")

        # Checking for data in destination database
        msg = self.samba3.db.search(expression="(cn=Niemand)")
        self.assertTrue(len(msg) >= 1)
        self.assertEquals(str(msg[0]["sambaSID"]), 
                "S-1-5-21-4231626423-2410014848-2360679739-2001")
        self.assertEquals(str(msg[0]["displayName"]), "Niemand")

        # Adding attribute...
        self.ldb.modify_ldif("""
dn: cn=Niemand,cn=Users,dc=vernstok,dc=nl
changetype: modify
add: description
description: Blah
""")

        # Checking whether changes are still there...
        msg = self.ldb.search(expression="(cn=Niemand)")
        self.assertTrue(len(msg) >= 1)
        self.assertEquals(str(msg[0]["cn"]), "Niemand")
        self.assertEquals(str(msg[0]["description"]), "Blah")

        # Modifying attribute...
        self.ldb.modify_ldif("""
dn: cn=Niemand,cn=Users,dc=vernstok,dc=nl
changetype: modify
replace: description
description: Blie
""")

        # Checking whether changes are still there...
        msg = self.ldb.search(expression="(cn=Niemand)")
        self.assertTrue(len(msg) >= 1)
        self.assertEquals(str(msg[0]["description"]), "Blie")

        # Deleting attribute...
        self.ldb.modify_ldif("""
dn: cn=Niemand,cn=Users,dc=vernstok,dc=nl
changetype: modify
delete: description
""")

        # Checking whether changes are no longer there...
        msg = self.ldb.search(expression="(cn=Niemand)")
        self.assertTrue(len(msg) >= 1)
        self.assertTrue(not "description" in msg[0])

        # Renaming record...
        self.ldb.rename("cn=Niemand,cn=Users,dc=vernstok,dc=nl", 
                        "cn=Niemand2,cn=Users,dc=vernstok,dc=nl")

        # Checking whether DN has changed...
        msg = self.ldb.search(expression="(cn=Niemand2)")
        self.assertEquals(len(msg), 1)
        self.assertEquals(str(msg[0].dn), 
                          "cn=Niemand2,cn=Users,dc=vernstok,dc=nl")

        # Deleting record...
        self.ldb.delete("cn=Niemand2,cn=Users,dc=vernstok,dc=nl")

        # Checking whether record is gone...
        msg = self.ldb.search(expression="(cn=Niemand2)")
        self.assertEquals(len(msg), 0)


class MapTestCase(MapBaseTestCase):

    def setUp(self):
        super(MapTestCase, self).setUp()
        ldb = Ldb(self.ldburl, lp=self.lp, session_info=system_session())
        ldif = read_datafile("provision_samba3sam.ldif")
        ldb.add_ldif(self.samba4.subst(ldif))
        self.setup_modules(ldb, self.samba3, self.samba4)
        del ldb
        self.ldb = Ldb(self.ldburl, lp=self.lp, session_info=system_session())

    def test_map_search(self):
        """Running search tests on mapped data."""
        self.samba3.db.add({
            "dn": "sambaDomainName=TESTS," + self.samba3.basedn,
            "objectclass": ["sambaDomain", "top"],
            "sambaSID": "S-1-5-21-4231626423-2410014848-2360679739",
            "sambaNextRid": "2000",
            "sambaDomainName": "TESTS"
            })

        # Add a set of split records
        self.ldb.add_ldif("""
dn: """+ self.samba4.dn("cn=Domain Users") + """
objectClass: group
cn: Domain Users
objectSid: S-1-5-21-4231626423-2410014848-2360679739-513
""")

        # Add a set of split records
        self.ldb.add_ldif("""
dn: """+ self.samba4.dn("cn=X") + """
objectClass: user
cn: X
codePage: x
revision: x
dnsHostName: x
nextRid: y
lastLogon: x
description: x
objectSid: S-1-5-21-4231626423-2410014848-2360679739-552
""")

        self.ldb.add({
            "dn": self.samba4.dn("cn=Y"),
            "objectClass": "top",
            "cn": "Y",
            "codePage": "x",
            "revision": "x",
            "dnsHostName": "y",
            "nextRid": "y",
            "lastLogon": "y",
            "description": "x"})

        self.ldb.add({
            "dn": self.samba4.dn("cn=Z"),
            "objectClass": "top",
            "cn": "Z",
            "codePage": "x",
            "revision": "y",
            "dnsHostName": "z",
            "nextRid": "y",
            "lastLogon": "z",
            "description": "y"})

        # Add a set of remote records

        self.samba3.db.add({
            "dn": self.samba3.dn("cn=A"),
            "objectClass": "posixAccount",
            "cn": "A",
            "sambaNextRid": "x",
            "sambaBadPasswordCount": "x", 
            "sambaLogonTime": "x",
            "description": "x",
            "sambaSID": "S-1-5-21-4231626423-2410014848-2360679739-552",
            "sambaPrimaryGroupSID": "S-1-5-21-4231626423-2410014848-2360679739-512"})

        self.samba3.db.add({
            "dn": self.samba3.dn("cn=B"),
            "objectClass": "top",
            "cn": "B",
            "sambaNextRid": "x",
            "sambaBadPasswordCount": "x",
            "sambaLogonTime": "y",
            "description": "x"})

        self.samba3.db.add({
            "dn": self.samba3.dn("cn=C"),
            "objectClass": "top",
            "cn": "C",
            "sambaNextRid": "x",
            "sambaBadPasswordCount": "y",
            "sambaLogonTime": "z",
            "description": "y"})

        # Testing search by DN

        # Search remote record by local DN
        dn = self.samba4.dn("cn=A")
        res = self.ldb.search(dn, scope=SCOPE_BASE, 
                attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "x")

        # Search remote record by remote DN
        dn = self.samba3.dn("cn=A")
        res = self.samba3.db.search(dn, scope=SCOPE_BASE, 
                attrs=["dnsHostName", "lastLogon", "sambaLogonTime"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertTrue(not "lastLogon" in res[0])
        self.assertEquals(str(res[0]["sambaLogonTime"]), "x")

        # Search split record by local DN
        dn = self.samba4.dn("cn=X")
        res = self.ldb.search(dn, scope=SCOPE_BASE, 
                attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["dnsHostName"]), "x")
        self.assertEquals(str(res[0]["lastLogon"]), "x")

        # Search split record by remote DN
        dn = self.samba3.dn("cn=X")
        res = self.samba3.db.search(dn, scope=SCOPE_BASE, 
                attrs=["dnsHostName", "lastLogon", "sambaLogonTime"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertTrue(not "lastLogon" in res[0])
        self.assertEquals(str(res[0]["sambaLogonTime"]), "x")

        # Testing search by attribute

        # Search by ignored attribute
        res = self.ldb.search(expression="(revision=x)", scope=SCOPE_DEFAULT, 
                attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")

        # Search by kept attribute
        res = self.ldb.search(expression="(description=y)", 
                scope=SCOPE_DEFAULT, attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[0]["dnsHostName"]), "z")
        self.assertEquals(str(res[0]["lastLogon"]), "z")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "z")

        # Search by renamed attribute
        res = self.ldb.search(expression="(badPwdCount=x)", scope=SCOPE_DEFAULT,
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")

        # Search by converted attribute
        # TODO:
        #   Using the SID directly in the parse tree leads to conversion
        #   errors, letting the search fail with no results.
        #res = self.ldb.search("(objectSid=S-1-5-21-4231626423-2410014848-2360679739-552)", scope=SCOPE_DEFAULT, attrs)
        res = self.ldb.search(expression="(objectSid=*)", base=None, scope=SCOPE_DEFAULT, attrs=["dnsHostName", "lastLogon", "objectSid"])
        self.assertEquals(len(res), 4)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[0]["dnsHostName"]), "x")
        self.assertEquals(str(res[0]["lastLogon"]), "x")
        self.assertSidEquals("S-1-5-21-4231626423-2410014848-2360679739-552", 
                             res[0]["objectSid"])
        self.assertTrue("objectSid" in res[0])
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertSidEquals("S-1-5-21-4231626423-2410014848-2360679739-552",
                             res[1]["objectSid"])
        self.assertTrue("objectSid" in res[1])

        # Search by generated attribute 
        # In most cases, this even works when the mapping is missing
        # a `convert_operator' by enumerating the remote db.
        res = self.ldb.search(expression="(primaryGroupID=512)", 
                           attrs=["dnsHostName", "lastLogon", "primaryGroupID"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "x")
        self.assertEquals(str(res[0]["primaryGroupID"]), "512")

        # Note that Xs "objectSid" seems to be fine in the previous search for
        # "objectSid"...
        #res = ldb.search(expression="(primaryGroupID=*)", NULL, ldb. SCOPE_DEFAULT, attrs)
        #print len(res) + " results found"
        #for i in range(len(res)):
        #    for (obj in res[i]) {
        #        print obj + ": " + res[i][obj]
        #    }
        #    print "---"
        #    

        # Search by remote name of renamed attribute */
        res = self.ldb.search(expression="(sambaBadPasswordCount=*)", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 0)

        # Search by objectClass
        attrs = ["dnsHostName", "lastLogon", "objectClass"]
        res = self.ldb.search(expression="(objectClass=user)", attrs=attrs)
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[0]["dnsHostName"]), "x")
        self.assertEquals(str(res[0]["lastLogon"]), "x")
        self.assertEquals(str(res[0]["objectClass"][0]), "user")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[1]["objectClass"][0]), "user")

        # Prove that the objectClass is actually used for the search
        res = self.ldb.search(expression="(|(objectClass=user)(badPwdCount=x))",
                              attrs=attrs)
        self.assertEquals(len(res), 3)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(set(res[0]["objectClass"]), set(["top"]))
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[1]["objectClass"][0]), "user")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[2])
        self.assertEquals(str(res[2]["lastLogon"]), "x")
        self.assertEquals(res[2]["objectClass"][0], "user")

        # Testing search by parse tree

        # Search by conjunction of local attributes
        res = self.ldb.search(expression="(&(codePage=x)(revision=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")

        # Search by conjunction of remote attributes
        res = self.ldb.search(expression="(&(lastLogon=x)(description=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[0]["dnsHostName"]), "x")
        self.assertEquals(str(res[0]["lastLogon"]), "x")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        
        # Search by conjunction of local and remote attribute 
        res = self.ldb.search(expression="(&(codePage=x)(description=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")

        # Search by conjunction of local and remote attribute w/o match
        attrs = ["dnsHostName", "lastLogon"]
        res = self.ldb.search(expression="(&(codePage=x)(nextRid=x))", 
                              attrs=attrs)
        self.assertEquals(len(res), 0)
        res = self.ldb.search(expression="(&(revision=x)(lastLogon=z))", 
                              attrs=attrs)
        self.assertEquals(len(res), 0)

        # Search by disjunction of local attributes
        res = self.ldb.search(expression="(|(revision=x)(dnsHostName=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 2)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")

        # Search by disjunction of remote attributes
        res = self.ldb.search(expression="(|(badPwdCount=x)(lastLogon=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 3)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertFalse("dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=A"))
        self.assertFalse("dnsHostName" in res[2])
        self.assertEquals(str(res[2]["lastLogon"]), "x")

        # Search by disjunction of local and remote attribute
        res = self.ldb.search(expression="(|(revision=x)(lastLogon=y))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 3)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=B"))
        self.assertFalse("dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "y")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[2]["dnsHostName"]), "x")
        self.assertEquals(str(res[2]["lastLogon"]), "x")

        # Search by disjunction of local and remote attribute w/o match
        res = self.ldb.search(expression="(|(codePage=y)(nextRid=z))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 0)

        # Search by negated local attribute
        res = self.ldb.search(expression="(!(revision=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 6)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[2]["dnsHostName"]), "z")
        self.assertEquals(str(res[2]["lastLogon"]), "z")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[3])
        self.assertEquals(str(res[3]["lastLogon"]), "z")

        # Search by negated remote attribute
        res = self.ldb.search(expression="(!(description=x))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 4)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[0]["dnsHostName"]), "z")
        self.assertEquals(str(res[0]["lastLogon"]), "z")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "z")

        # Search by negated conjunction of local attributes
        res = self.ldb.search(expression="(!(&(codePage=x)(revision=x)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 6)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[2]["dnsHostName"]), "z")
        self.assertEquals(str(res[2]["lastLogon"]), "z")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[3])
        self.assertEquals(str(res[3]["lastLogon"]), "z")

        # Search by negated conjunction of remote attributes
        res = self.ldb.search(expression="(!(&(lastLogon=x)(description=x)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 6)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "y")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[2]["dnsHostName"]), "z")
        self.assertEquals(str(res[2]["lastLogon"]), "z")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[3])
        self.assertEquals(str(res[3]["lastLogon"]), "z")

        # Search by negated conjunction of local and remote attribute
        res = self.ldb.search(expression="(!(&(codePage=x)(description=x)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 6)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[2]["dnsHostName"]), "z")
        self.assertEquals(str(res[2]["lastLogon"]), "z")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[3])
        self.assertEquals(str(res[3]["lastLogon"]), "z")

        # Search by negated disjunction of local attributes
        res = self.ldb.search(expression="(!(|(revision=x)(dnsHostName=x)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[1])
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[2]["dnsHostName"]), "z")
        self.assertEquals(str(res[2]["lastLogon"]), "z")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[3])
        self.assertEquals(str(res[3]["lastLogon"]), "z")

        # Search by negated disjunction of remote attributes
        res = self.ldb.search(expression="(!(|(badPwdCount=x)(lastLogon=x)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 5)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=Y"))
        self.assertEquals(str(res[0]["dnsHostName"]), "y")
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[1]["dnsHostName"]), "z")
        self.assertEquals(str(res[1]["lastLogon"]), "z")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[2])
        self.assertEquals(str(res[2]["lastLogon"]), "z")

        # Search by negated disjunction of local and remote attribute
        res = self.ldb.search(expression="(!(|(revision=x)(lastLogon=y)))", 
                              attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 5)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "x")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[1]["dnsHostName"]), "z")
        self.assertEquals(str(res[1]["lastLogon"]), "z")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[2])
        self.assertEquals(str(res[2]["lastLogon"]), "z")

        # Search by complex parse tree
        res = self.ldb.search(expression="(|(&(revision=x)(dnsHostName=x))(!(&(description=x)(nextRid=y)))(badPwdCount=y))", attrs=["dnsHostName", "lastLogon"])
        self.assertEquals(len(res), 7)
        self.assertEquals(str(res[0].dn), self.samba4.dn("cn=B"))
        self.assertTrue(not "dnsHostName" in res[0])
        self.assertEquals(str(res[0]["lastLogon"]), "y")
        self.assertEquals(str(res[1].dn), self.samba4.dn("cn=X"))
        self.assertEquals(str(res[1]["dnsHostName"]), "x")
        self.assertEquals(str(res[1]["lastLogon"]), "x")
        self.assertEquals(str(res[2].dn), self.samba4.dn("cn=A"))
        self.assertTrue(not "dnsHostName" in res[2])
        self.assertEquals(str(res[2]["lastLogon"]), "x")
        self.assertEquals(str(res[3].dn), self.samba4.dn("cn=Z"))
        self.assertEquals(str(res[3]["dnsHostName"]), "z")
        self.assertEquals(str(res[3]["lastLogon"]), "z")
        self.assertEquals(str(res[4].dn), self.samba4.dn("cn=C"))
        self.assertTrue(not "dnsHostName" in res[4])
        self.assertEquals(str(res[4]["lastLogon"]), "z")

        # Clean up
        dns = [self.samba4.dn("cn=%s" % n) for n in ["A","B","C","X","Y","Z"]]
        for dn in dns:
            self.ldb.delete(dn)

    def test_map_modify_local(self):
        """Modification of local records."""
        # Add local record
        dn = "cn=test,dc=idealx,dc=org"
        self.ldb.add({"dn": dn, 
                 "cn": "test",
                 "foo": "bar",
                 "revision": "1",
                 "description": "test"})
        # Check it's there
        attrs = ["foo", "revision", "description"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["foo"]), "bar")
        self.assertEquals(str(res[0]["revision"]), "1")
        self.assertEquals(str(res[0]["description"]), "test")
        # Check it's not in the local db
        res = self.samba4.db.search(expression="(cn=test)", 
                                    scope=SCOPE_DEFAULT, attrs=attrs)
        self.assertEquals(len(res), 0)
        # Check it's not in the remote db
        res = self.samba3.db.search(expression="(cn=test)", 
                                    scope=SCOPE_DEFAULT, attrs=attrs)
        self.assertEquals(len(res), 0)

        # Modify local record
        ldif = """
dn: """ + dn + """
replace: foo
foo: baz
replace: description
description: foo
"""
        self.ldb.modify_ldif(ldif)
        # Check in local db
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["foo"]), "baz")
        self.assertEquals(str(res[0]["revision"]), "1")
        self.assertEquals(str(res[0]["description"]), "foo")

        # Rename local record
        dn2 = "cn=toast,dc=idealx,dc=org"
        self.ldb.rename(dn, dn2)
        # Check in local db
        res = self.ldb.search(dn2, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["foo"]), "baz")
        self.assertEquals(str(res[0]["revision"]), "1")
        self.assertEquals(str(res[0]["description"]), "foo")

        # Delete local record
        self.ldb.delete(dn2)
        # Check it's gone
        res = self.ldb.search(dn2, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)

    def test_map_modify_remote_remote(self):
        """Modification of remote data of remote records"""
        # Add remote record
        dn = self.samba4.dn("cn=test")
        dn2 = self.samba3.dn("cn=test")
        self.samba3.db.add({"dn": dn2, 
                   "cn": "test",
                   "description": "foo",
                   "sambaBadPasswordCount": "3",
                   "sambaNextRid": "1001"})
        # Check it's there
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, 
                attrs=["description", "sambaBadPasswordCount", "sambaNextRid"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "foo")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "3")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")
        # Check in mapped db
        attrs = ["description", "badPwdCount", "nextRid"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs, expression="")
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "foo")
        self.assertEquals(str(res[0]["badPwdCount"]), "3")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 0)

        # Modify remote data of remote record
        ldif = """
dn: """ + dn + """
replace: description
description: test
replace: badPwdCount
badPwdCount: 4
"""
        self.ldb.modify_ldif(ldif)
        # Check in mapped db
        res = self.ldb.search(dn, scope=SCOPE_BASE, 
                attrs=["description", "badPwdCount", "nextRid"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["badPwdCount"]), "4")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        # Check in remote db
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, 
                attrs=["description", "sambaBadPasswordCount", "sambaNextRid"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "4")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")

        # Rename remote record
        dn2 = self.samba4.dn("cn=toast")
        self.ldb.rename(dn, dn2)
        # Check in mapped db
        dn = dn2
        res = self.ldb.search(dn, scope=SCOPE_BASE, 
                attrs=["description", "badPwdCount", "nextRid"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["badPwdCount"]), "4")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        # Check in remote db 
        dn2 = self.samba3.dn("cn=toast")
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, 
                attrs=["description", "sambaBadPasswordCount", "sambaNextRid"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "4")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")

        # Delete remote record
        self.ldb.delete(dn)
        # Check in mapped db that it's removed
        res = self.ldb.search(dn, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)
        # Check in remote db
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)

    def test_map_modify_remote_local(self):
        """Modification of local data of remote records"""
        # Add remote record (same as before)
        dn = self.samba4.dn("cn=test")
        dn2 = self.samba3.dn("cn=test")
        self.samba3.db.add({"dn": dn2, 
                   "cn": "test",
                   "description": "foo",
                   "sambaBadPasswordCount": "3",
                   "sambaNextRid": "1001"})

        # Modify local data of remote record
        ldif = """
dn: """ + dn + """
add: revision
revision: 1
replace: description
description: test

"""
        self.ldb.modify_ldif(ldif)
        # Check in mapped db
        attrs = ["revision", "description"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["revision"]), "1")
        # Check in remote db
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertTrue(not "revision" in res[0])
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "description" in res[0])
        self.assertEquals(str(res[0]["revision"]), "1")

        # Delete (newly) split record
        self.ldb.delete(dn)

    def test_map_modify_split(self):
        """Testing modification of split records"""
        # Add split record
        dn = self.samba4.dn("cn=test")
        dn2 = self.samba3.dn("cn=test")
        self.ldb.add({
            "dn": dn,
            "cn": "test",
            "description": "foo",
            "badPwdCount": "3",
            "nextRid": "1001",
            "revision": "1"})
        # Check it's there
        attrs = ["description", "badPwdCount", "nextRid", "revision"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "foo")
        self.assertEquals(str(res[0]["badPwdCount"]), "3")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        self.assertEquals(str(res[0]["revision"]), "1")
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "description" in res[0])
        self.assertTrue(not "badPwdCount" in res[0])
        self.assertTrue(not "nextRid" in res[0])
        self.assertEquals(str(res[0]["revision"]), "1")
        # Check in remote db
        attrs = ["description", "sambaBadPasswordCount", "sambaNextRid", 
                 "revision"]
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "foo")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "3")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")
        self.assertTrue(not "revision" in res[0])

        # Modify of split record
        ldif = """
dn: """ + dn + """
replace: description
description: test
replace: badPwdCount
badPwdCount: 4
replace: revision
revision: 2
"""
        self.ldb.modify_ldif(ldif)
        # Check in mapped db
        attrs = ["description", "badPwdCount", "nextRid", "revision"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["badPwdCount"]), "4")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        self.assertEquals(str(res[0]["revision"]), "2")
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "description" in res[0])
        self.assertTrue(not "badPwdCount" in res[0])
        self.assertTrue(not "nextRid" in res[0])
        self.assertEquals(str(res[0]["revision"]), "2")
        # Check in remote db
        attrs = ["description", "sambaBadPasswordCount", "sambaNextRid", 
                 "revision"]
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "4")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")
        self.assertTrue(not "revision" in res[0])

        # Rename split record
        dn2 = self.samba4.dn("cn=toast")
        self.ldb.rename(dn, dn2)
        # Check in mapped db
        dn = dn2
        attrs = ["description", "badPwdCount", "nextRid", "revision"]
        res = self.ldb.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["badPwdCount"]), "4")
        self.assertEquals(str(res[0]["nextRid"]), "1001")
        self.assertEquals(str(res[0]["revision"]), "2")
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE, attrs=attrs)
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn)
        self.assertTrue(not "description" in res[0])
        self.assertTrue(not "badPwdCount" in res[0])
        self.assertTrue(not "nextRid" in res[0])
        self.assertEquals(str(res[0]["revision"]), "2")
        # Check in remote db
        dn2 = self.samba3.dn("cn=toast")
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE, 
          attrs=["description", "sambaBadPasswordCount", "sambaNextRid", 
                 "revision"])
        self.assertEquals(len(res), 1)
        self.assertEquals(str(res[0].dn), dn2)
        self.assertEquals(str(res[0]["description"]), "test")
        self.assertEquals(str(res[0]["sambaBadPasswordCount"]), "4")
        self.assertEquals(str(res[0]["sambaNextRid"]), "1001")
        self.assertTrue(not "revision" in res[0])

        # Delete split record
        self.ldb.delete(dn)
        # Check in mapped db
        res = self.ldb.search(dn, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)
        # Check in local db
        res = self.samba4.db.search(dn, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)
        # Check in remote db
        res = self.samba3.db.search(dn2, scope=SCOPE_BASE)
        self.assertEquals(len(res), 0)
