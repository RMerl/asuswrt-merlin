#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Tests various schema replication scenarios
#
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
#  export DC1=dc1_dns_name
#  export DC2=dc2_dns_name
#  export SUBUNITRUN=$samba4srcdir/scripting/bin/subunitrun
#  PYTHONPATH="$PYTHONPATH:$samba4srcdir/torture/drs/python" $SUBUNITRUN repl_schema -U"$DOMAIN/$DC_USERNAME"%"$DC_PASSWORD"
#

import time
import random

from ldb import (
    ERR_NO_SUCH_OBJECT,
    LdbError,
    SCOPE_BASE,
    Message,
    FLAG_MOD_ADD,
    FLAG_MOD_REPLACE,
    )

import drs_base


class DrsReplSchemaTestCase(drs_base.DrsBaseTestCase):

    # prefix for all objects created
    obj_prefix = None
    # current Class or Attribute object id
    obj_id = 0

    def setUp(self):
        super(DrsReplSchemaTestCase, self).setUp()

        # initialize objects prefix if not done yet
        if self.obj_prefix is None:
            t = time.strftime("%s", time.gmtime())
            DrsReplSchemaTestCase.obj_prefix = "DrsReplSchema-%s" % t

    def tearDown(self):
        super(DrsReplSchemaTestCase, self).tearDown()

    def _make_obj_names(self, base_name):
        '''Try to create a unique name for an object
           that is to be added to schema'''
        self.obj_id += 1
        obj_name = "%s-%d-%s" % (self.obj_prefix, self.obj_id, base_name)
        obj_ldn = obj_name.replace("-", "")
        obj_dn = "CN=%s,%s" % (obj_name, self.schema_dn)
        return (obj_dn, obj_name, obj_ldn)

    def _schema_new_class(self, ldb_ctx, base_name, attrs=None):
        (class_dn, class_name, class_ldn) = self._make_obj_names(base_name)
        rec = {"dn": class_dn,
               "objectClass": ["top", "classSchema"],
               "cn": class_name,
               "lDAPDisplayName": class_ldn,
               "governsId": "1.2.840." + str(random.randint(1,100000)) + ".1.5.13",
               "instanceType": "4",
               "objectClassCategory": "1",
               "subClassOf": "top",
               "systemOnly": "FALSE"}
        # allow overriding/adding attributes
        if not attrs is None:
            rec.update(attrs)
        # add it to the Schema
        ldb_ctx.add(rec)
        self._ldap_schemaUpdateNow(ldb_ctx)
        return (rec["lDAPDisplayName"], rec["dn"])

    def _schema_new_attr(self, ldb_ctx, base_name, attrs=None):
        (attr_dn, attr_name, attr_ldn) = self._make_obj_names(base_name)
        rec = {"dn": attr_dn,
               "objectClass": ["top", "attributeSchema"],
               "cn": attr_name,
               "lDAPDisplayName": attr_ldn,
               "attributeId": "1.2.841." + str(random.randint(1,100000)) + ".1.5.13",
               "attributeSyntax": "2.5.5.12",
               "omSyntax": "64",
               "instanceType": "4",
               "isSingleValued": "TRUE",
               "systemOnly": "FALSE"}
        # allow overriding/adding attributes
        if not attrs is None:
            rec.update(attrs)
        # add it to the Schema
        ldb_ctx.add(rec)
        self._ldap_schemaUpdateNow(ldb_ctx)
        return (rec["lDAPDisplayName"], rec["dn"])

    def _check_object(self, obj_dn):
        '''Check if object obj_dn exists on both DCs'''
        res_dc1 = self.ldb_dc1.search(base=obj_dn,
                                      scope=SCOPE_BASE,
                                      attrs=["*"])
        self.assertEquals(len(res_dc1), 1,
                          "%s doesn't exists on %s" % (obj_dn, self.dnsname_dc1))
        try:
            res_dc2 = self.ldb_dc2.search(base=obj_dn,
                                          scope=SCOPE_BASE,
                                          attrs=["*"])
        except LdbError, (enum, estr):
            if enum == ERR_NO_SUCH_OBJECT:
                self.fail("%s doesn't exists on %s" % (obj_dn, self.dnsname_dc2))
            raise
        self.assertEquals(len(res_dc2), 1,
                          "%s doesn't exists on %s" % (obj_dn, self.dnsname_dc2))

    def test_class(self):
        """Simple test for classSchema replication"""
        # add new classSchema object
        (c_ldn, c_dn) = self._schema_new_class(self.ldb_dc1, "cls-S")
        # force replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, nc_dn=self.schema_dn)
        # check object is replicated
        self._check_object(c_dn)

    def test_classInheritance(self):
        """Test inheritance through subClassOf
           I think 5 levels of inheritance is pretty decent for now."""
        # add 5 levels deep hierarchy
        c_dn_list = []
        c_ldn_last = None
        for i in range(1, 6):
            base_name = "cls-I-%02d" % i
            (c_ldn, c_dn) = self._schema_new_class(self.ldb_dc1, base_name)
            c_dn_list.append(c_dn)
            if c_ldn_last:
                # inherit from last class added
                m = Message.from_dict(self.ldb_dc1,
                                      {"dn": c_dn,
                                       "subClassOf": c_ldn_last},
                                      FLAG_MOD_REPLACE)
                self.ldb_dc1.modify(m)
            # store last class ldapDisplayName
            c_ldn_last = c_ldn
        # force replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, nc_dn=self.schema_dn)
        # check objects are replicated
        for c_dn in c_dn_list:
            self._check_object(c_dn)

    def test_classWithCustomAttribute(self):
        """Create new Attribute and a Class,
           that has value for newly created attribute.
           This should check code path that searches for
           AttributeID_id in Schema cache"""
        # add new attributeSchema object
        (a_ldn, a_dn) = self._schema_new_attr(self.ldb_dc1, "attr-A")
        # add a base classSchema class so we can use our new
        # attribute in class definition in a sibling class
        (c_ldn, c_dn) = self._schema_new_class(self.ldb_dc1, "cls-A",
                                               {"systemMayContain": a_ldn})
        # add new classSchema object with value for a_ldb attribute
        (c_ldn, c_dn) = self._schema_new_class(self.ldb_dc1, "cls-B",
                                               {"objectClass": ["top", "classSchema", c_ldn],
                                                a_ldn: "test_classWithCustomAttribute"})
        # force replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, nc_dn=self.schema_dn)
        # check objects are replicated
        self._check_object(c_dn)
        self._check_object(a_dn)

    def test_attribute(self):
        """Simple test for attributeSchema replication"""
        # add new attributeSchema object
        (a_ldn, a_dn) = self._schema_new_attr(self.ldb_dc1, "attr-S")
        # force replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, nc_dn=self.schema_dn)
        # check object is replicated
        self._check_object(a_dn)

    def test_all(self):
        """Basic plan is to create bunch of classSchema
           and attributeSchema objects, replicate Schema NC
           and then check all objects are replicated correctly"""

        # add new classSchema object
        (c_ldn, c_dn) = self._schema_new_class(self.ldb_dc1, "cls-A")
        # add new attributeSchema object
        (a_ldn, a_dn) = self._schema_new_attr(self.ldb_dc1, "attr-A")

        # add attribute to the class we have
        m = Message.from_dict(self.ldb_dc1,
                              {"dn": c_dn,
                               "mayContain": a_ldn},
                              FLAG_MOD_ADD)
        self.ldb_dc1.modify(m)

        # force replication from DC1 to DC2
        self._net_drs_replicate(DC=self.dnsname_dc2, fromDC=self.dnsname_dc1, nc_dn=self.schema_dn)
        
        # check objects are replicated
        self._check_object(c_dn)
        self._check_object(a_dn)
