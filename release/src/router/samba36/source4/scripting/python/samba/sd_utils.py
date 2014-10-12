#!/usr/bin/env python
#
# Utility methods for security descriptor manipulation
#
# Copyright Nadezhda Ivanova 2010 <nivanova@samba.org>
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

import samba
from ldb import Message, MessageElement, Dn
from ldb import FLAG_MOD_REPLACE, SCOPE_BASE
from samba.ndr import ndr_pack, ndr_unpack
from samba.dcerpc import security

class SDUtils:
    '''Some utilities for manipulation of security descriptors
    on objects'''

    def __init__(self, samdb):
        self.ldb = samdb
        self.domain_sid = security.dom_sid(self.ldb.get_domain_sid())

    def modify_sd_on_dn(self, object_dn, sd, controls=None):
        """ Modify security descriptor using either SDDL string
            or security.descriptor object
        """
        m = Message()
        m.dn = Dn(self.ldb, object_dn)
        assert(isinstance(sd, str) or isinstance(sd, security.descriptor))
        if isinstance(sd, str):
            tmp_desc = security.descriptor.from_sddl(sd, self.domain_sid)
        elif isinstance(sd, security.descriptor):
            tmp_desc = sd

        m["nTSecurityDescriptor"] = MessageElement(ndr_pack(tmp_desc),
                                                       FLAG_MOD_REPLACE,
                                                       "nTSecurityDescriptor")
        self.ldb.modify(m, controls)

    def read_sd_on_dn(self, object_dn, controls=None):
        res = self.ldb.search(object_dn, SCOPE_BASE, None,
                              ["nTSecurityDescriptor"], controls=controls)
        desc = res[0]["nTSecurityDescriptor"][0]
        return ndr_unpack(security.descriptor, desc)

    def get_object_sid(self, object_dn):
        res = self.ldb.search(object_dn)
        return ndr_unpack(security.dom_sid, res[0]["objectSid"][0])

    def dacl_add_ace(self, object_dn, ace):
        """ Adds an ACE to an objects security descriptor
        """
        desc = self.read_sd_on_dn(object_dn)
        desc_sddl = desc.as_sddl(self.domain_sid)
        if ace in desc_sddl:
            return
        if desc_sddl.find("(") >= 0:
            desc_sddl = desc_sddl[:desc_sddl.index("(")] + ace + desc_sddl[desc_sddl.index("("):]
        else:
            desc_sddl = desc_sddl + ace
        self.modify_sd_on_dn(object_dn, desc_sddl)

    def get_sd_as_sddl(self, object_dn, controls=None):
        """ Return object nTSecutiryDescriptor in SDDL format
        """
        desc = self.read_sd_on_dn(object_dn, controls=controls)
        return desc.as_sddl(self.domain_sid)
