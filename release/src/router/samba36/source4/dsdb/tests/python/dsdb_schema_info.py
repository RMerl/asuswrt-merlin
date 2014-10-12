#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Unix SMB/CIFS implementation.
# Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010
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

#
# Usage:
#  export DC_SERVER=target_dc_or_local_samdb_url
#  export SUBUNITRUN=$samba4srcdir/scripting/bin/subunitrun
#  PYTHONPATH="$PYTHONPATH:$samba4srcdir/lib/ldb/tests/python" $SUBUNITRUN dsdb_schema_info -U"$DOMAIN/$DC_USERNAME"%"$DC_PASSWORD"
#

import sys
import time
import random

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")

from ldb import SCOPE_BASE, LdbError

import samba.tests
import samba.dcerpc.drsuapi
from samba.dcerpc.drsblobs import schemaInfoBlob
from samba.ndr import ndr_unpack
from samba.dcerpc.misc import GUID


class SchemaInfoTestCase(samba.tests.TestCase):

    # static SamDB connection
    sam_db = None

    def setUp(self):
        super(SchemaInfoTestCase, self).setUp()

        # connect SamDB if we haven't yet
        if self.sam_db is None:
            ldb_url = samba.tests.env_get_var_value("DC_SERVER")
            SchemaInfoTestCase.sam_db = samba.tests.connect_samdb(ldb_url)

        # fetch rootDSE
        res = self.sam_db.search(base="", expression="", scope=SCOPE_BASE, attrs=["*"])
        self.assertEquals(len(res), 1)
        self.schema_dn = res[0]["schemaNamingContext"][0]
        self.base_dn = res[0]["defaultNamingContext"][0]
        self.forest_level = int(res[0]["forestFunctionality"][0])

        # get DC invocation_id
        self.invocation_id = GUID(self.sam_db.get_invocation_id())

    def tearDown(self):
        super(SchemaInfoTestCase, self).tearDown()

    def _getSchemaInfo(self):
        try:
            schema_info_data = self.sam_db.searchone(attribute="schemaInfo",
                                                     basedn=self.schema_dn,
                                                     expression="(objectClass=*)",
                                                     scope=SCOPE_BASE)
            self.assertEqual(len(schema_info_data), 21)
            schema_info = ndr_unpack(schemaInfoBlob, schema_info_data)
            self.assertEqual(schema_info.marker, 0xFF)
        except KeyError:
            # create default schemaInfo if
            # attribute value is not created yet
            schema_info = schemaInfoBlob()
            schema_info.revision = 0
            schema_info.invocation_id = self.invocation_id
        return schema_info

    def _checkSchemaInfo(self, schi_before, schi_after):
        self.assertEqual(schi_before.revision + 1, schi_after.revision)
        self.assertEqual(schi_before.invocation_id, schi_after.invocation_id)
        self.assertEqual(schi_after.invocation_id, self.invocation_id)

    def _ldap_schemaUpdateNow(self):
        ldif = """
dn:
changetype: modify
add: schemaUpdateNow
schemaUpdateNow: 1
"""
        self.sam_db.modify_ldif(ldif)

    def _make_obj_names(self, prefix):
        obj_name = prefix + time.strftime("%s", time.gmtime())
        obj_ldap_name = obj_name.replace("-", "")
        obj_dn = "CN=%s,%s" % (obj_name, self.schema_dn)
        return (obj_name, obj_ldap_name, obj_dn)

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

    def test_AddModifyAttribute(self):
        # get initial schemaInfo
        schi_before = self._getSchemaInfo()

        # create names for an attribute to add
        (attr_name, attr_ldap_name, attr_dn) = self._make_obj_names("schemaInfo-Attr-")
        ldif = self._make_attr_ldif(attr_name, attr_dn)

        # add the new attribute
        self.sam_db.add_ldif(ldif)
        self._ldap_schemaUpdateNow()
        # compare resulting schemaInfo
        schi_after = self._getSchemaInfo()
        self._checkSchemaInfo(schi_before, schi_after)

        # rename the Attribute
        attr_dn_new = attr_dn.replace(attr_name, attr_name + "-NEW")
        try:
            self.sam_db.rename(attr_dn, attr_dn_new)
        except LdbError, (num, _):
            self.fail("failed to change lDAPDisplayName for %s: %s" % (attr_name, _))

        # compare resulting schemaInfo
        schi_after = self._getSchemaInfo()
        self._checkSchemaInfo(schi_before, schi_after)
        pass


    def _make_class_ldif(self, class_name, class_dn):
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

    def test_AddModifyClass(self):
        # get initial schemaInfo
        schi_before = self._getSchemaInfo()

        # create names for a Class to add
        (class_name, class_ldap_name, class_dn) = self._make_obj_names("schemaInfo-Class-")
        ldif = self._make_class_ldif(class_name, class_dn)

        # add the new Class
        self.sam_db.add_ldif(ldif)
        self._ldap_schemaUpdateNow()
        # compare resulting schemaInfo
        schi_after = self._getSchemaInfo()
        self._checkSchemaInfo(schi_before, schi_after)

        # rename the Class
        class_dn_new = class_dn.replace(class_name, class_name + "-NEW")
        try:
            self.sam_db.rename(class_dn, class_dn_new)
        except LdbError, (num, _):
            self.fail("failed to change lDAPDisplayName for %s: %s" % (class_name, _))

        # compare resulting schemaInfo
        schi_after = self._getSchemaInfo()
        self._checkSchemaInfo(schi_before, schi_after)

